module.exports = [
  {
    "type": "heading",
    "defaultValue": "TouchyWeather Face"
  },
  {
    "type": "text",
    "defaultValue": "The weather watch face with a whole deck under the glass. Nudge the watch (flick your wrist or firmly tap the glass) to page through your weather."
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
        "defaultValue": "Nudge Gesture"
      },
      {
        "type": "select",
        "messageKey": "GestureMode",
        "label": "What a nudge does",
        "description": "A nudge is a wrist flick or a firm tap on the watch. Nudge Deck pages through your peek cards; Single peek shows one dense overlay; Auto-rotate cycles pages on a timer with no gestures.",
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
        "defaultValue": "Clock Face"
      },
      {
        "type": "select",
        "messageKey": "BatteryDisplay",
        "label": "Battery indicator",
        "defaultValue": "1",
        "options": [
          { "label": "Off", "value": "0" },
          { "label": "Always show", "value": "1" },
          { "label": "Only when low", "value": "2" }
        ]
      },
      {
        "type": "select",
        "messageKey": "Complication",
        "label": "Complication",
        "description": "An extra reading shown on the clock face, below the date.",
        "defaultValue": "0",
        "options": [
          { "label": "None", "value": "0" },
          { "label": "Feels like", "value": "1" },
          { "label": "Wind", "value": "2" },
          { "label": "Humidity", "value": "3" },
          { "label": "UV index", "value": "4" },
          { "label": "Air quality", "value": "5" },
          { "label": "Step count", "value": "6" }
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
        "messageKey": "UVBadge",
        "label": "UV badge",
        "description": "Show a small UV badge beside the temperature when the UV index is High or above.",
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
        "defaultValue": "Units"
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
