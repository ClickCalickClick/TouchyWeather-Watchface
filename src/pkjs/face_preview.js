// Clay customFn: wires the live resting-face schematic (Phase 8).
//
// Runs inside the Clay config page (serialized with toSource — must be
// closure-free, ES5). The drawing lives in the faceSchematic component
// (face_schematic.js); this only connects it to the settings: after the page
// builds, it reads the eight layout-affecting inputs (the two line slots, two
// badge slots, both "only when notable" toggles, and UpdatedDisplay — plus the
// watch shape/B&W, which the component derives from clayConfig.meta), attaches
// a change listener to each, and re-renders the sketch on every change so
// boxes appear, disappear and recentre live.
module.exports = function(minified) {
  var clayConfig = this;

  // Keys that reshape the resting face. Order is documentation only.
  var LAYOUT_KEYS = [
    'Complication',   // Line 1  → COMPS row
    'Complication2',  // Line 2  → COMPS row
    'BadgeComp1',     // Badge 1 → BADGES row
    'BadgeComp2',     // Badge 2 → BADGES row
    'Badge1Notable',  // Badge 1 conditional style
    'Badge2Notable',  // Badge 2 conditional style
    'UpdatedDisplay'  // UPDATED row: Always / stale-only / Off
  ];

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var items = clayConfig.getItemsByType('faceSchematic');
    if (!items.length || typeof items[0].render !== 'function') { return; }
    var preview = items[0];

    function toInt(v) { var n = parseInt(v, 10); return isNaN(n) ? 0 : n; }

    function readState() {
      function get(key) {
        var item = clayConfig.getItemByMessageKey(key);
        return item ? item.get() : null;
      }
      return {
        line1: toInt(get('Complication')),
        line2: toInt(get('Complication2')),
        badge1: toInt(get('BadgeComp1')),
        badge2: toInt(get('BadgeComp2')),
        notable1: !!get('Badge1Notable'),
        notable2: !!get('Badge2Notable'),
        updated: toInt(get('UpdatedDisplay'))
      };
    }

    function redraw() { preview.render(readState()); }

    for (var i = 0; i < LAYOUT_KEYS.length; i++) {
      var item = clayConfig.getItemByMessageKey(LAYOUT_KEYS[i]);
      if (item && typeof item.on === 'function') { item.on('change', redraw); }
    }

    redraw(); // initial paint
  });
};
