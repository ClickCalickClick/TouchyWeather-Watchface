module.exports = [
  {
    "type": "heading",
    "defaultValue": "TouchyWeather Face"
  },
  {
    "type": "text",
    "defaultValue": "The weather watch face with a whole deck under the glass. Nudge the watch — a flick of the wrist — to page through your weather. (Prefer a firm tap on the watch? Switch the nudge input below.)"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Appearance"
      },
      {
        "type": "radiogroup",
        "messageKey": "Theme",
        "label": "Theme",
        "defaultValue": "0",
        "options": [
          { "label": "Light", "value": "0" },
          { "label": "Dark", "value": "1" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "AnimationsEnabled",
        "label": "Animations",
        "description": "Animate the weather icon after a nudge or fresh data, settling to a static frame a few seconds later to save battery.",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Clock Face"
      },
      {
        "type": "text",
        "defaultValue": "Four slots you can fill with any reading — weather, step count or the watch battery: two text lines under the date, and two colored pills below the weather. Whatever you leave off, the rest of the face grows to fill the space."
      },
      {
        "type": "faceSchematic",
        "id": "faceSchematic",
        "label": "Live preview",
        "description": "A rough sketch of the resting face with your current settings — boxes appear, disappear and recenter as you change the slots below. It follows your connected watch's shape and screen; a dashed pill only shows when its reading is notable."
      },
      {
        "type": "select",
        "messageKey": "Complication",
        "label": "Line 1",
        "description": "A reading shown as text below the date.",
        "defaultValue": "1",
        "options": [
          { "label": "None", "value": "0" },
          { "label": "Feels like", "value": "1" },
          { "label": "Wind", "value": "2" },
          { "label": "Humidity", "value": "3" },
          { "label": "Dew point", "value": "7" },
          { "label": "UV index", "value": "4" },
          { "label": "Air quality", "value": "5" },
          { "label": "Rain chance", "value": "8" },
          { "label": "Step count", "value": "6" },
          { "label": "Watch battery", "value": "9" }
        ]
      },
      {
        "type": "select",
        "messageKey": "Complication2",
        "label": "Line 2",
        "description": "An optional second reading. When both lines are set, they appear side by side.",
        "defaultValue": "0",
        "options": [
          { "label": "None", "value": "0" },
          { "label": "Feels like", "value": "1" },
          { "label": "Wind", "value": "2" },
          { "label": "Humidity", "value": "3" },
          { "label": "Dew point", "value": "7" },
          { "label": "UV index", "value": "4" },
          { "label": "Air quality", "value": "5" },
          { "label": "Rain chance", "value": "8" },
          { "label": "Step count", "value": "6" },
          { "label": "Watch battery", "value": "9" }
        ]
      },
      {
        "type": "select",
        "messageKey": "BadgeComp1",
        "label": "Badge 1",
        "description": "A reading shown as a colored pill below the weather. Step count isn't available here — it's too wide for a pill on smaller watches.",
        "defaultValue": "8",
        "options": [
          { "label": "None", "value": "0" },
          { "label": "Rain chance", "value": "8" },
          { "label": "Feels like", "value": "1" },
          { "label": "Wind", "value": "2" },
          { "label": "Humidity", "value": "3" },
          { "label": "Dew point", "value": "7" },
          { "label": "UV index", "value": "4" },
          { "label": "Air quality", "value": "5" },
          { "label": "Watch battery", "value": "9" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "Badge1Notable",
        "label": "Badge 1 only when notable",
        "description": "Hide this pill unless the reading is worth a look — rain at 50% or more, UV 6+ around midday, air quality above 100, watch battery at 20% or less (or charging), and similar thresholds for the other readings.",
        "defaultValue": false
      },
      {
        "type": "select",
        "messageKey": "BadgeComp2",
        "label": "Badge 2",
        "description": "An optional second pill, shown beside the first.",
        "defaultValue": "0",
        "options": [
          { "label": "None", "value": "0" },
          { "label": "Rain chance", "value": "8" },
          { "label": "Feels like", "value": "1" },
          { "label": "Wind", "value": "2" },
          { "label": "Humidity", "value": "3" },
          { "label": "Dew point", "value": "7" },
          { "label": "UV index", "value": "4" },
          { "label": "Air quality", "value": "5" },
          { "label": "Watch battery", "value": "9" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "Badge2Notable",
        "label": "Badge 2 only when notable",
        "description": "Same as above, for the second pill.",
        "defaultValue": false
      },
      {
        "type": "radiogroup",
        "messageKey": "ClockEmphasis",
        "label": "Clock size",
        "description": "Balanced gives the time and the temperature equal weight. Large clock shrinks the weather row so the time can grow — it only takes effect where the bigger clock genuinely fits, so on a full face you may need to turn a slot or the 'last updated' pill off to see it.",
        "defaultValue": "0",
        "options": [
          { "label": "Balanced", "value": "0" },
          { "label": "Large clock", "value": "1" }
        ]
      },
      {
        "type": "select",
        "messageKey": "UpdatedDisplay",
        "label": "Last updated",
        "description": "The 'UPDATED 5M AGO' pill at the bottom. Turn it off and the rest of the face grows to fill the space. An imminent-rain alert always takes this spot over, whatever you pick here.",
        "defaultValue": "0",
        "options": [
          { "label": "Always show", "value": "0" },
          { "label": "Only when stale", "value": "1" },
          { "label": "Off", "value": "2" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Nudge Gesture"
      },
      {
        "type": "select",
        "messageKey": "GestureMode",
        "label": "What a nudge does",
        "description": "A nudge is a wrist flick or a firm tap on the watch — pick which under Nudge input. Nudge Deck deals your enabled Peek Pages one per nudge; Single peek shows one fixed view you choose below (Peek Pages don't apply); Auto-rotate cycles the enabled pages on a timer, no nudging needed.",
        "defaultValue": "0",
        "options": [
          { "label": "Nudge Deck (page through cards)", "value": "0" },
          { "label": "Single peek overlay", "value": "1" },
          { "label": "Auto-rotate pages", "value": "2" },
          { "label": "Off (clock only)", "value": "3" }
        ]
      },
      {
        "type": "select",
        "messageKey": "SinglePeekView",
        "label": "Single peek shows",
        "description": "Which view a Single peek nudge opens. The overlay packs everything onto one screen; pick a single page instead if you only ever want that one. Only applies to the Single peek mode.",
        "defaultValue": "0",
        "options": [
          { "label": "Everything overlay", "value": "0" },
          { "label": "6 Hours", "value": "1" },
          { "label": "Week Ahead", "value": "2" },
          { "label": "Conditions", "value": "3" },
          { "label": "Sun + Moon", "value": "4" }
        ]
      },
      {
        "type": "select",
        "messageKey": "TapInputMode",
        "label": "Nudge input",
        "description": "Which motion the face reacts to. Both use the accelerometer (a watchface can't use the touchscreen). Wrist flick reacts to turning your wrist; Tap reacts to tapping the watch face/body; Either accepts both (most reliable, but more prone to accidental triggers).",
        "defaultValue": "0",
        "options": [
          { "label": "Wrist flick", "value": "0" },
          { "label": "Tap the watch", "value": "1" },
          { "label": "Either", "value": "2" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Peek Pages"
      },
      {
        "type": "text",
        "id": "peekPagesNote",
        "defaultValue": "Used by Nudge Deck and Auto-rotate — not by Single peek or Off."
      },
      {
        "type": "toggle",
        "messageKey": "PageEnabledHours",
        "label": "6 Hours",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "PageEnabledWeek",
        "label": "Week Ahead",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "PageEnabledConditions",
        "label": "Conditions",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "PageEnabledSunMoon",
        "label": "Sun + Moon",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Ambient Extras"
      },
      {
        "type": "toggle",
        "messageKey": "RainAutoShow",
        "label": "Rain auto-peek",
        "description": "When rain is due within the hour, the face flashes the hourly page on its own.",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "NightMode",
        "label": "Night mode",
        "description": "Between sunset and sunrise, switch to the dark theme and show the moon phase in place of the condition icon.",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "QuickViewReflow",
        "label": "Quick View reflow",
        "description": "When a timeline event covers the bottom of the face, compact the clock upward instead of being obscured.",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Units & Format"
      },
      {
        "type": "radiogroup",
        "messageKey": "Units",
        "label": "Measurement system",
        "defaultValue": "0",
        "options": [
          { "label": "Imperial (°F, mph)", "value": "0" },
          { "label": "Metric (°C, km/h)",  "value": "1" }
        ]
      },
      {
        "type": "select",
        "messageKey": "WindSpeedUnit",
        "label": "Wind speed",
        "defaultValue": "auto",
        "description": "Wind can use a different unit from the temperature. Automatic follows the measurement system above (mph with Imperial, km/h with Metric); metres per second is the everyday wind unit across Scandinavia and much of Europe.",
        "options": [
          { "label": "Automatic",           "value": "auto" },
          { "label": "Miles per hour (mph)", "value": "mph" },
          { "label": "Kilometres per hour (km/h)", "value": "kmh" },
          { "label": "Metres per second (m/s)",    "value": "ms" }
        ]
      },
      {
        "type": "radiogroup",
        "messageKey": "TimeFormat",
        "label": "Forecast time format",
        "description": "Formats the hourly labels and sun times. The clock itself always follows the watch's system setting.",
        "defaultValue": "0",
        "options": [
          { "label": "Match watch",     "value": "0" },
          { "label": "12-hour (2 PM)",  "value": "1" },
          { "label": "24-hour (14:00)", "value": "2" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "UseDewPoint",
        "label": "Show dew point",
        "description": "Replace the humidity reading on the Conditions page with dew point temperature.",
        "defaultValue": false
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Location"
      },
      {
        "type": "input",
        "messageKey": "LocationOverride",
        "label": "Location override",
        "description": "Leave blank to use your phone's location. Or enter \"lat,lon\" (e.g. 40.71,-74.01) for a fixed location.",
        "defaultValue": ""
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
