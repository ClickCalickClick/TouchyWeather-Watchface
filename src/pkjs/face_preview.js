// Clay customFn: wires the live resting-face schematic (Phase 8), keeps the
// four complication slots from offering a reading another slot already has,
// and greys out the Peek Pages toggles when the gesture mode ignores them.
//
// Runs inside the Clay config page (serialized with toSource — must be
// closure-free, ES5). The drawing lives in the faceSchematic component
// (face_schematic.js); this only connects it to the settings: after the page
// builds, it reads the eight layout-affecting inputs (the two line slots, two
// badge slots, both "only when notable" toggles, and UpdatedDisplay — plus the
// watch shape/B&W, which the component derives from clayConfig.meta), attaches
// a change listener to each, and re-renders the sketch on every change so
// boxes appear, disappear and recentre live.
//
// The second AFTER_BUILD handler disables the four Peek Pages toggles and shows
// a note whenever GestureMode is Single peek ('1') or Off ('3'), since only
// Nudge Deck ('0') and Auto-rotate ('2') actually deal the peek pages.
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

  // Each reading can only be in ONE of the four slots. Showing "FEELS 75°"
  // as both a line and a pill is never what someone means, and the watch
  // draws a duplicated pair once anyway (clock_zone.c's draw-time guard), so
  // an option another slot already holds would silently do nothing. Disable
  // and hide it in the other three pickers instead, and re-run on every
  // change so freeing a reading offers it back everywhere.
  //
  // "None" (value '0') is never excluded — any number of slots may be off.
  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var SLOT_KEYS = ['Complication', 'Complication2', 'BadgeComp1', 'BadgeComp2'];

    // The rendered <select> — Clay puts data-manipulator-target on it, so the
    // item's $manipulatorTarget IS the element (same idiom face_schematic.js
    // uses for its canvas).
    function slotSelect(key) {
      var item = clayConfig.getItemByMessageKey(key);
      if (!item || !item.$manipulatorTarget) { return null; }
      var node = item.$manipulatorTarget[0];
      return (node && node.tagName === 'SELECT') ? node : null;
    }

    function applyExclusions() {
      // value -> the slot holding it. Later slots lose a tie, but a tie can
      // only exist in a config saved before this rule (or hand-sent).
      var owner = {};
      var i, j, item, v;
      for (i = 0; i < SLOT_KEYS.length; i++) {
        item = clayConfig.getItemByMessageKey(SLOT_KEYS[i]);
        if (!item) { continue; }
        v = String(item.get());
        if (v && v !== '0' && !owner[v]) { owner[v] = SLOT_KEYS[i]; }
      }

      for (i = 0; i < SLOT_KEYS.length; i++) {
        var sel = slotSelect(SLOT_KEYS[i]);
        if (!sel) { continue; }
        for (j = 0; j < sel.options.length; j++) {
          var opt = sel.options[j];
          var taken = owner[opt.value] && owner[opt.value] !== SLOT_KEYS[i];
          opt.disabled = !!taken;
          opt.hidden = !!taken;
        }
      }
    }

    for (var k = 0; k < SLOT_KEYS.length; k++) {
      var slotItem = clayConfig.getItemByMessageKey(SLOT_KEYS[k]);
      if (slotItem && typeof slotItem.on === 'function') {
        slotItem.on('change', applyExclusions);
      }
    }

    applyExclusions(); // apply the saved slots on first build
  });

  // Grey out the Peek Pages toggles for the gesture modes that don't use them.
  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var PAGE_KEYS = ['PageEnabledHours', 'PageEnabledWeek',
                     'PageEnabledConditions', 'PageEnabledSunMoon'];
    var mode = clayConfig.getItemByMessageKey('GestureMode');
    if (!mode) { return; }
    var note = clayConfig.getItemById('peekPagesNote');

    // The Single-peek view picker is the mirror image: only that one mode uses
    // it, so it greys out whenever the peek pages are live (and vice versa).
    var single = clayConfig.getItemByMessageKey('SinglePeekView');

    function applyPagesState() {
      // Only Nudge Deck ('0') and Auto-rotate ('2') deal the peek pages.
      var usesPages = (mode.get() === '0' || mode.get() === '2');
      for (var i = 0; i < PAGE_KEYS.length; i++) {
        var item = clayConfig.getItemByMessageKey(PAGE_KEYS[i]);
        if (!item) { continue; }
        if (usesPages) { item.enable(); } else { item.disable(); }
      }
      if (note) { if (usesPages) { note.hide(); } else { note.show(); } }
      if (single) {
        if (mode.get() === '1') { single.enable(); } else { single.disable(); }
      }
    }

    mode.on('change', applyPagesState);
    applyPagesState(); // apply the saved gesture mode on first build
  });
};
