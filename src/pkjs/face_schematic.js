// Custom Clay component: live resting-face schematic ("faceSchematic").
//
// A canvas that sketches the resting face as stacked ROW BOXES —
// TIME · DATE · COMPS · WEATHER · BADGES · UPDATED — so the phone-side
// config page shows, before saving, how the four complication slots and the
// UPDATED row reshape the face. Schematic, NOT pixel-faithful (CLAUDE.md /
// master plan P8): boxes appear, disappear and recenter as the slots toggle,
// the whole thing follows the connected watch's shape (round vs rect) and
// screen (colour vs 1-bit B&W). The wiring — which keys trigger a redraw —
// lives in face_preview.js (the Clay customFn); this file only draws.
//
// IMPORTANT: Clay serializes this whole object into the config page with
// toSource(), so every function below must be self-contained — no closures
// over this module's (or PKJS's) scope, ES5 only (old phone webviews). All of
// the drawing therefore lives inside initialize(), which attaches a
// `render(state)` method to the ClayItem for face_preview.js to call. The
// component's own value is meaningless (display-only), so the manipulator is a
// no-op that never touches the canvas.
module.exports = {
  name: 'faceSchematic',
  template: [
    '<div class="component component-face-schematic">',
    '  {{if label}}<span class="label">{{{label}}}</span>{{/if}}',
    '  <div class="face-schematic-stage">',
    '    <canvas class="face-schematic-canvas" width="480" height="480"',
    '            data-manipulator-target></canvas>',
    '  </div>',
    '  {{if description}}<div class="description">{{{description}}}</div>{{/if}}',
    '</div>'
  ].join('\n'),
  style: [
    '.component-face-schematic { padding: 8px 10px 12px; }',
    '.component-face-schematic .label { display: block; padding: 6px 0 2px; }',
    '.component-face-schematic .face-schematic-stage { text-align: center; }',
    '.component-face-schematic .face-schematic-canvas {',
    '  display: inline-block;',
    '  width: 240px;',
    '  height: 240px;',
    '  max-width: 100%;',
    '}',
    '.component-face-schematic .description { padding-top: 8px; }'
  ].join('\n'),
  defaults: {
    label: '',
    description: ''
  },
  // Display-only: nothing to persist, and set() must never clear the canvas
  // (Clay calls set('') once during build).
  manipulator: {
    get: function() { return ''; },
    set: function() { return this; }
  },
  initialize: function(minified, clayConfig) {
    var self = this;
    var canvas = self.$manipulatorTarget[0];
    var ctx = canvas.getContext('2d');

    // Logical drawing space (CSS px); backing store is scaled up for the
    // device pixel ratio so the sketch stays crisp on retina phones.
    var LOGICAL = 240;
    var dpr = (typeof window !== 'undefined' && window.devicePixelRatio) || 1;
    canvas.width = LOGICAL * dpr;
    canvas.height = LOGICAL * dpr;
    canvas.style.width = LOGICAL + 'px';
    canvas.style.height = LOGICAL + 'px';

    // Connected watch → shape/colour/geometry. It never changes during a
    // config session, so capture it once. Unknown / no watch (e.g. a plain
    // browser preview) falls back to small rectangular colour (basalt).
    var watch = (clayConfig && clayConfig.meta &&
                 clayConfig.meta.activeWatchInfo) || null;

    // -- Pure helpers (all self-contained; see toSource note above) --------

    // Native display geometry per platform. chalk + gabbro are the two round
    // classes (gabbro is large-round 260); diorite + flint are the 1-bit B&W
    // classes. Mirrors src/c/ui.h's screen-class axis.
    function platformInfo(w) {
      var p = (w && w.platform) || '';
      var table = {
        basalt:  { shape: 'rect',  bw: false, w: 144, h: 168 },
        chalk:   { shape: 'round', bw: false, w: 180, h: 180 },
        diorite: { shape: 'rect',  bw: true,  w: 144, h: 168 },
        emery:   { shape: 'rect',  bw: false, w: 200, h: 228 },
        flint:   { shape: 'rect',  bw: true,  w: 144, h: 168 },
        gabbro:  { shape: 'round', bw: false, w: 260, h: 260 }
      };
      return table[p] || table.basalt;
    }

    // Complication value (matches config.js / settings.h) → sample label.
    function readingText(v) {
      if (v === 1) return 'FEELS 54°';
      if (v === 2) return 'WIND 8';
      if (v === 3) return 'HUM 60%';
      if (v === 4) return 'UV 7';
      if (v === 5) return 'AQI 42';
      if (v === 6) return 'STEPS 5K';
      if (v === 7) return 'DEW 51°';
      if (v === 8) return 'RAIN 60%';
      return '';
    }

    // Badge fill family, mirroring clock_zone.c's prv_format_badge:
    // RAIN/HUM/DEW/AQI → blue, UV/FEELS → orange, WIND → neutral.
    function badgeKind(v) {
      if (v === 4 || v === 1) return 'orange';
      if (v === 2) return 'wind';
      return 'blue';
    }

    function palette(bw) {
      if (bw) {
        return {
          bw: true,
          body: '#000000', edge: '#ffffff', ink: '#ffffff', dim: '#c9c9c9',
          boxLine: 'rgba(255,255,255,0.42)', boxFill: 'rgba(255,255,255,0.05)',
          pill: { blue: '#ffffff', orange: '#ffffff', wind: '#ffffff' },
          pillText: '#000000', icon: '#ffffff'
        };
      }
      return {
        bw: false,
        body: '#161a20', edge: '#39404a', ink: '#eef2f6', dim: '#96a0ab',
        boxLine: 'rgba(255,255,255,0.16)', boxFill: 'rgba(255,255,255,0.05)',
        pill: { blue: '#2f6fed', orange: '#f5883a', wind: '#5b6672' },
        pillText: '#ffffff', icon: '#f5b53a'
      };
    }

    // Presence solver — which rows exist, top to bottom. TIME/DATE/WEATHER
    // are always present; COMPS iff a line slot is set; BADGES iff a badge
    // slot is set; UPDATED unless UpdatedDisplay is Off (2). Rain-alert
    // takeover is a live-only concern, not represented in the preview.
    function computeRows(state) {
      var rows = ['TIME', 'DATE'];
      if (state.line1 || state.line2) rows.push('COMPS');
      rows.push('WEATHER');
      if (state.badge1 || state.badge2) rows.push('BADGES');
      if (state.updated !== 2) rows.push('UPDATED');
      return rows;
    }

    // Stack the present rows with elastic gaps, promote TIME as optional rows
    // drop away (mimics the flow layout's XL clock promotion), scale down to
    // fit tight classes, and centre the whole stack vertically. Widths are
    // chord-clamped on round. Returns boxes in native face coordinates.
    function layoutRows(rows, face) {
      var base = { TIME: 40, DATE: 18, COMPS: 16, WEATHER: 36,
                   BADGES: 20, UPDATED: 18 };
      var pad = face.shape === 'round' ? Math.round(face.h * 0.14) : 10;
      var avail = face.h - 2 * pad;

      var optional = 0, i;
      for (i = 0; i < rows.length; i++) {
        if (rows[i] === 'COMPS' || rows[i] === 'BADGES' ||
            rows[i] === 'UPDATED') { optional++; }
      }

      var heights = [];
      for (i = 0; i < rows.length; i++) {
        var h = base[rows[i]];
        if (rows[i] === 'TIME') { h += (3 - optional) * 6; } // promote
        heights.push(h);
      }

      var gap = 6;
      var n = rows.length;
      var content = gap * (n - 1);
      for (i = 0; i < heights.length; i++) { content += heights[i]; }
      var scale = content > avail ? avail / content : 1;

      var used = gap * scale * (n - 1);
      for (i = 0; i < heights.length; i++) { used += heights[i] * scale; }

      var y = (face.h - used) / 2;
      var cx = face.w / 2;
      var boxes = [];
      for (i = 0; i < rows.length; i++) {
        var rh = heights[i] * scale;
        boxes.push({
          key: rows[i], cx: cx, y: y, h: rh,
          maxW: rowMaxWidth(face, y, rh)
        });
        y += rh + gap * scale;
      }
      return boxes;
    }

    // Widest content a row may use. On round it is the chord at the row's
    // worse (further-from-centre) edge, minus a bezel inset; on rect it is
    // the face width minus symmetric margins.
    function rowMaxWidth(face, y, h) {
      if (face.shape !== 'round') { return face.w - 24; }
      var r = face.w / 2;
      var cy = face.h / 2;
      var d = Math.max(Math.abs(y - cy), Math.abs(y + h - cy));
      var half = d >= r ? 0 : Math.sqrt(r * r - d * d);
      return Math.max(0, 2 * half - 16);
    }

    // -- Canvas primitives -------------------------------------------------

    function roundRect(c, x, y, w, h, r) {
      r = Math.min(r, w / 2, h / 2);
      c.beginPath();
      c.moveTo(x + r, y);
      c.arcTo(x + w, y, x + w, y + h, r);
      c.arcTo(x + w, y + h, x, y + h, r);
      c.arcTo(x, y + h, x, y, r);
      c.arcTo(x, y, x + w, y, r);
      c.closePath();
    }

    // Shrink font until `text` fits `maxW`, floored at `min`.
    function fitFont(c, text, weight, px, maxW, min) {
      var size = px;
      while (size > min) {
        c.font = weight + ' ' + size + 'px -apple-system, Roboto, sans-serif';
        if (c.measureText(text).width <= maxW) break;
        size -= 1;
      }
      c.font = weight + ' ' + size + 'px -apple-system, Roboto, sans-serif';
      return size;
    }

    // A faint structural row box with a centred label.
    function drawStructBox(c, box, pal, text, weight, fontPx) {
      if (!text) return;
      var innerMax = box.maxW - 10;
      if (innerMax < 12) innerMax = 12;
      var size = fitFont(c, text, weight, fontPx, innerMax, 8);
      var tw = c.measureText(text).width;
      var boxW = Math.min(box.maxW, tw + 16);
      var x = box.cx - boxW / 2;
      roundRect(c, x, box.y, boxW, box.h, Math.min(6, box.h / 2));
      c.fillStyle = pal.boxFill;
      c.fill();
      c.lineWidth = 1;
      c.strokeStyle = pal.boxLine;
      c.stroke();
      c.fillStyle = pal.ink;
      c.textAlign = 'center';
      c.textBaseline = 'middle';
      c.fillText(text, box.cx, box.y + box.h / 2 + 0.5);
      return { x: x, w: boxW, fontPx: size };
    }

    // A pill. Solid = always shown; "conditional" (dashed, hollow) = only
    // shown when notable / when stale — so the notable toggles and the
    // UpdatedDisplay "only when stale" choice are visible in the sketch.
    function drawPill(c, cx, cy, h, text, fill, textCol, conditional, maxW) {
      var fontPx = Math.max(8, Math.round(h * 0.52));
      var innerMax = (maxW || 999) - 14;
      fontPx = fitFont(c, text, 'bold', fontPx, innerMax, 8);
      var tw = c.measureText(text).width;
      var w = tw + 14;
      var x = cx - w / 2;
      var y = cy - h / 2;
      roundRect(c, x, y, w, h, h / 2);
      if (conditional) {
        c.save();
        if (c.setLineDash) c.setLineDash([4, 3]);
        c.globalAlpha = 0.16;
        c.fillStyle = fill;
        c.fill();
        c.globalAlpha = 1;
        c.lineWidth = 1.4;
        c.strokeStyle = fill;
        c.stroke();
        c.restore();
        c.fillStyle = fill;
      } else {
        c.fillStyle = fill;
        c.fill();
        c.fillStyle = textCol;
      }
      c.textAlign = 'center';
      c.textBaseline = 'middle';
      c.fillText(text, cx, cy + 0.5);
      return w;
    }

    // A row of one or two pills, centred as a group and chord-clamped.
    function drawBadgeRow(c, box, pal, badges) {
      var gap = 6;
      var pillH = Math.min(box.h, 20);
      var cy = box.y + box.h / 2;
      // Provisional widths so the group can be centred.
      var each = badges.length > 1 ? (box.maxW - gap) / 2 : box.maxW;
      var widths = [], i;
      for (i = 0; i < badges.length; i++) {
        var fontPx = Math.max(8, Math.round(pillH * 0.52));
        fitFont(c, badges[i].text, 'bold', fontPx, each - 14, 8);
        widths.push(Math.min(each, c.measureText(badges[i].text).width + 14));
      }
      var total = 0;
      for (i = 0; i < widths.length; i++) total += widths[i];
      total += gap * (badges.length - 1);
      var x = box.cx - total / 2;
      for (i = 0; i < badges.length; i++) {
        var cxp = x + widths[i] / 2;
        var col = pal.pill[badges[i].kind] || pal.pill.blue;
        drawPill(c, cxp, cy, pillH, badges[i].text, col, pal.pillText,
                 badges[i].notable, widths[i] + 2);
        x += widths[i] + gap;
      }
    }

    function drawWeather(c, box, pal) {
      // Small "sun" dot + sample temps, framed like the other struct rows.
      var text = '72°  75|60';
      var size = fitFont(c, text, 'bold', Math.round(box.h * 0.5),
                         box.maxW - 30, 8);
      var tw = c.measureText(text).width;
      var dotR = Math.max(3, box.h * 0.22);
      var groupW = Math.min(box.maxW, tw + dotR * 2 + 20);
      var x = box.cx - groupW / 2;
      roundRect(c, x, box.y, groupW, box.h, Math.min(6, box.h / 2));
      c.fillStyle = pal.boxFill;
      c.fill();
      c.lineWidth = 1;
      c.strokeStyle = pal.boxLine;
      c.stroke();
      var cyc = box.y + box.h / 2;
      c.beginPath();
      c.arc(x + 8 + dotR, cyc, dotR, 0, Math.PI * 2);
      c.fillStyle = pal.icon;
      c.fill();
      c.fillStyle = pal.ink;
      c.textAlign = 'left';
      c.textBaseline = 'middle';
      c.font = 'bold ' + size + 'px -apple-system, Roboto, sans-serif';
      c.fillText(text, x + 8 + dotR * 2 + 6, cyc + 0.5);
    }

    // -- The one public entry point ---------------------------------------

    self.render = function(state) {
      var face = platformInfo(watch);
      var pal = palette(face.bw);

      // Fit the native face into the logical canvas, preserving aspect.
      var margin = 6;
      var s = Math.min((LOGICAL - 2 * margin) / face.w,
                       (LOGICAL - 2 * margin) / face.h);
      var ox = (LOGICAL - face.w * s) / 2;
      var oy = (LOGICAL - face.h * s) / 2;

      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.clearRect(0, 0, LOGICAL, LOGICAL);

      ctx.save();
      ctx.translate(ox, oy);
      ctx.scale(s, s);

      // Watch body (+ clip so nothing spills past the glass on round).
      if (face.shape === 'round') {
        ctx.beginPath();
        ctx.arc(face.w / 2, face.h / 2, face.w / 2, 0, Math.PI * 2);
      } else {
        roundRect(ctx, 0, 0, face.w, face.h, 12);
      }
      ctx.fillStyle = pal.body;
      ctx.fill();
      ctx.lineWidth = 2;
      ctx.strokeStyle = pal.edge;
      ctx.stroke();
      ctx.save();
      ctx.clip();

      var rows = computeRows(state);
      var boxes = layoutRows(rows, face);

      var badges = [];
      if (state.badge1) {
        badges.push({ text: readingText(state.badge1),
                      kind: badgeKind(state.badge1), notable: state.notable1 });
      }
      if (state.badge2) {
        badges.push({ text: readingText(state.badge2),
                      kind: badgeKind(state.badge2), notable: state.notable2 });
      }

      var lines = [];
      if (state.line1) lines.push(readingText(state.line1));
      if (state.line2) lines.push(readingText(state.line2));

      var i;
      for (i = 0; i < boxes.length; i++) {
        var b = boxes[i];
        if (b.key === 'TIME') {
          drawStructBox(ctx, b, pal, '12:34', 'bold',
                        Math.round(b.h * 0.86));
        } else if (b.key === 'DATE') {
          drawStructBox(ctx, b, pal, 'MON JUL 22', 'bold',
                        Math.round(b.h * 0.82));
        } else if (b.key === 'COMPS') {
          drawStructBox(ctx, b, pal, lines.join('   ·   '), '600',
                        Math.round(b.h * 0.8));
        } else if (b.key === 'WEATHER') {
          drawWeather(ctx, b, pal);
        } else if (b.key === 'BADGES') {
          drawBadgeRow(ctx, b, pal, badges);
        } else if (b.key === 'UPDATED') {
          // UpdatedDisplay: 0 Always → solid, 1 Only-when-stale → conditional.
          drawPill(ctx, b.cx, b.y + b.h / 2, Math.min(b.h, 18),
                   'UPDATED 5M', pal.dim,
                   pal.bw ? '#000000' : '#20242b',
                   state.updated === 1, b.maxW);
        }
      }

      ctx.restore(); // clip
      ctx.restore(); // face transform

      // Expose the resolved sketch for the headless reactivity test. Harmless
      // in production (config webview only).
      if (typeof window !== 'undefined') {
        window.__lastSchematic = {
          shape: face.shape, bw: face.bw, platform: (watch && watch.platform) || null,
          rows: rows.slice(),
          lines: lines.slice(),
          badges: badges.map(function(x) {
            return { kind: x.kind, notable: x.notable, text: x.text };
          }),
          updatedMode: state.updated
        };
      }
    };
  }
};
