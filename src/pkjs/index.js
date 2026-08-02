// TouchyWeather Face PKJS — Open-Meteo fetcher, trimmed from the
// TouchyWeather app's pipeline. No API key required. Drops the app's radar
// streaming, pollen proxy, analytics, golden-hour math and card-state
// seeding; keeps forecast + air quality + geocode + moon + Clay.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
// Phase 8: live resting-face schematic in the phone settings. The customFn
// (face_preview) wires change listeners; the faceSchematic component draws it.
// Must be registered before generateUrl().
var clay = new Clay(clayConfig, require('./face_preview'),
                    { autoHandleEvents: false });
clay.registerComponent(require('./face_schematic'));

var COND = {
  SUNNY: 0, PARTLY_CLOUDY: 1, CLOUDY: 2, RAIN: 3, SNOW: 4, STORM: 5, FOG: 6
};

// Map Open-Meteo WMO weather codes to our internal enum.
function mapWeatherCode(code) {
  if (code === 0) return COND.SUNNY;
  if (code === 1 || code === 2) return COND.PARTLY_CLOUDY;
  if (code === 3) return COND.CLOUDY;
  if (code >= 45 && code <= 48) return COND.FOG;
  if (code >= 51 && code <= 67) return COND.RAIN;
  if (code >= 71 && code <= 77) return COND.SNOW;
  if (code >= 80 && code <= 82) return COND.RAIN;
  if (code >= 85 && code <= 86) return COND.SNOW;
  if (code >= 95 && code <= 99) return COND.STORM;
  return COND.PARTLY_CLOUDY;
}

function degToCompass(deg) {
  var dirs = ['N','NE','E','SE','S','SW','W','NW'];
  return dirs[Math.round(deg / 45) % 8];
}

// Resolve the active time format. "1"/"2" are explicit overrides; "0"
// (default, "Match watch") follows the watch's system clock style, which
// the C side reports via ClockIs24h on each refresh request.
function use24h() {
  var tf = localStorage.getItem('timeFormat') || '0';
  if (tf === '1') return false;            // 12-hour
  if (tf === '2') return true;             // 24-hour
  return localStorage.getItem('clockIs24h') === '1';  // match watch
}

function fmtTime12(iso) {
  if (!iso) return '';
  var t = iso.split('T')[1] || '';
  var parts = t.split(':');
  var h = parseInt(parts[0], 10);
  var m = parts[1] || '00';
  if (use24h()) {
    return (h < 10 ? '0' + h : h) + ':' + m;
  }
  var ampm = h >= 12 ? 'PM' : 'AM';
  h = h % 12; if (h === 0) h = 12;
  return h + ':' + m + ' ' + ampm;
}

// Moon phase from Julian-date formula (Open-Meteo doesn't provide it).
// Reference new moon: 2000-01-06 18:14 UTC = JD 2451550.1.
function computeMoonPhase(date) {
  var ms = date.getTime();
  var jd = ms / 86400000.0 + 2440587.5;
  var p = (jd - 2451550.1) / 29.530588853;
  p = p - Math.floor(p); // 0..1
  var illum = Math.round((1 - Math.cos(p * 2 * Math.PI)) * 50);
  var phase, name1, name2;
  if      (p < 0.03)  { phase = 0; name1 = 'NEW';     name2 = 'MOON'; }
  else if (p < 0.22)  { phase = 1; name1 = 'WAXING';  name2 = 'CRESCENT'; }
  else if (p < 0.28)  { phase = 2; name1 = 'FIRST';   name2 = 'QUARTER'; }
  else if (p < 0.47)  { phase = 3; name1 = 'WAXING';  name2 = 'GIBBOUS'; }
  else if (p < 0.53)  { phase = 4; name1 = 'FULL';    name2 = 'MOON'; }
  else if (p < 0.72)  { phase = 5; name1 = 'WANING';  name2 = 'GIBBOUS'; }
  else if (p < 0.78)  { phase = 6; name1 = 'LAST';    name2 = 'QUARTER'; }
  else if (p < 0.97)  { phase = 7; name1 = 'WANING';  name2 = 'CRESCENT'; }
  else                { phase = 0; name1 = 'NEW';     name2 = 'MOON'; }
  return { phase: phase, illum: illum, name1: name1, name2: name2 };
}

function getUnits() {
  return localStorage.getItem('units') === 'metric' ? 'metric' : 'imperial';
}

function xhr(url, cb) {
  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.timeout = 15000;
  req.onload = function() {
    if (req.status >= 200 && req.status < 300) {
      try { cb(null, JSON.parse(req.responseText)); }
      catch (e) { cb(e); }
    } else {
      cb(new Error('HTTP ' + req.status));
    }
  };
  req.onerror = function() { cb(new Error('xhr error')); };
  req.ontimeout = function() { cb(new Error('xhr timeout')); };
  req.send();
}

// ----------------------------------------------------------------------
// Anonymous active-user analytics (ported from the TouchyWeather app).
//
// Identifies the user by Pebble's account token — a stable, per-user,
// per-app value containing no name/email/PII. When that's unavailable
// (user not signed in) we fall back to a random id persisted locally.
// The raw id is sent over HTTPS and hashed server-side; only aggregate
// DAU/WAU/MAU/YAU counts + a coarse (~11 km) location cell are stored.
// Throttled to one ping per UTC day so "daily active" is the natural unit.
// Fire-and-forget: failures never affect weather. The `variant: 'face'`
// tag lets the dashboard tell face users apart from app users (the app
// sends 'app'); see proxy/api/track.js.
// ----------------------------------------------------------------------

// Anonymous analytics ping. Same Vercel project + RADAR_SECRET auth key as
// the app's radar/pollen/track endpoints. The server hashes the id and stores
// only aggregate counts — nothing identifiable leaves the device.
// The key lives in gitignored secrets.js (template: secrets.js.example);
// without it the ping just 401s and analytics is silently skipped.
var PROXY_KEY = require('./secrets').PROXY_KEY;
var TRACK_PROXY_URL = 'https://touchyweather-radar-proxy.vercel.app/api/track' +
  (PROXY_KEY ? '?key=' + PROXY_KEY : '');

function utcDayStr() {
  var d = new Date();
  function p(n) { return n < 10 ? '0' + n : '' + n; }
  return d.getUTCFullYear() + '-' + p(d.getUTCMonth() + 1) + '-' + p(d.getUTCDate());
}

function getAnalyticsId() {
  // Prefer the anonymous Pebble account token (consistent across the same
  // user's watches). It can be '' when the user isn't signed in.
  var token = '';
  try { token = Pebble.getAccountToken() || ''; } catch (e) { token = ''; }
  if (token) return token;

  // Fallback: a locally persisted random id so an unsigned-in user still
  // counts as one stable device rather than a new user every launch.
  var anon = localStorage.getItem('anonId');
  if (!anon) {
    anon = 'anon-';
    for (var i = 0; i < 32; i++) {
      anon += Math.floor(Math.random() * 16).toString(16);
    }
    localStorage.setItem('anonId', anon);
  }
  return anon;
}

function trackPing(lat, lon) {
  var today = utcDayStr();
  if (localStorage.getItem('lastPingDay') === today) return; // already counted today

  var id = getAnalyticsId();
  // Round coords to 0.1° (~11 km) before they leave the device — analytics
  // never needs precise location, only a coarse heatmap cell.
  var rLat = Math.round(lat * 10) / 10;
  var rLon = Math.round(lon * 10) / 10;

  try {
    var req = new XMLHttpRequest();
    req.open('POST', TRACK_PROXY_URL, true);
    req.timeout = 15000;
    req.setRequestHeader('Content-Type', 'application/json');
    req.onload = function() {
      if (req.status >= 200 && req.status < 300) {
        // Mark the day done only on success so a failed ping retries next refresh.
        localStorage.setItem('lastPingDay', today);
        console.log('track sent');
      } else {
        console.log('track http ' + req.status);
      }
    };
    req.onerror = function() { console.log('track err'); };
    req.ontimeout = function() { console.log('track timeout'); };
    req.send(JSON.stringify({ id: id, lat: rLat, lon: rLon, variant: 'face' }));
  } catch (e) {
    console.log('track exception: ' + e.message);
  }
}

// Best-effort reverse geocode (BigDataCloud, keyless), cached by ~1.1km
// cell for 24h. Always calls `done` with a string; never blocks weather.
var GEO_TTL_MS = 24 * 60 * 60 * 1000;

function reverseGeocode(lat, lon, done) {
  var cellKey = lat.toFixed(2) + ',' + lon.toFixed(2);
  var cachedName = localStorage.getItem('lastLocationName') || '';
  var cachedAt = parseInt(localStorage.getItem('geoCacheAt') || '0', 10);
  if (cachedName && cachedAt > 0 &&
      localStorage.getItem('geoCacheCoords') === cellKey &&
      Date.now() - cachedAt < GEO_TTL_MS) {
    done(cachedName);
    return;
  }
  var url = 'https://api.bigdatacloud.net/data/reverse-geocode-client' +
            '?latitude=' + lat + '&longitude=' + lon + '&localityLanguage=en';
  xhr(url, function(err, data) {
    if (err || !data) {
      done(localStorage.getItem('lastLocationName') || '');
      return;
    }
    var name = data.city || data.locality || data.principalSubdivision || '';
    if (name) {
      localStorage.setItem('lastLocationName', name);
      localStorage.setItem('geoCacheCoords', cellKey);
      localStorage.setItem('geoCacheAt', String(Date.now()));
    } else {
      name = localStorage.getItem('lastLocationName') || '';
    }
    done(name);
  });
}

function fetchWeather(lat, lon) {
  var units = getUnits();
  var tempUnit = units === 'metric' ? 'celsius' : 'fahrenheit';
  var windUnit = units === 'metric' ? 'kmh' : 'mph';

  // `precipitation` stays in the hourly request: RainAlertMinutes is driven
  // by measurable amount (matching the app), even though per-hour amounts
  // aren't sent to the face.
  var fc = 'https://api.open-meteo.com/v1/forecast' +
    '?latitude=' + lat + '&longitude=' + lon +
    '&current=temperature_2m,apparent_temperature,relative_humidity_2m,dew_point_2m,weather_code,wind_speed_10m,wind_direction_10m,uv_index' +
    '&hourly=temperature_2m,weather_code,precipitation_probability,precipitation' +
    '&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max' +
    '&temperature_unit=' + tempUnit +
    '&wind_speed_unit=' + windUnit +
    '&timezone=auto&forecast_days=5';

  var aq = 'https://air-quality-api.open-meteo.com/v1/air-quality' +
    '?latitude=' + lat + '&longitude=' + lon +
    '&current=us_aqi&timezone=auto';

  reverseGeocode(lat, lon, function(locName) {
  xhr(fc, function(err, data) {
    if (err) { console.log('forecast err: ' + err.message); fetchDone(); return; }
    xhr(aq, function(_e2, aqd) {
      var msg = {};
      msg.LocationName = (locName || '').substring(0, 31);
      try {
        var cur = data.current || {};
        var daily = data.daily || {};
        var hourly = data.hourly || {};
        msg.Temp = Math.round(cur.temperature_2m);
        msg.FeelsLike = Math.round(cur.apparent_temperature);
        msg.Humidity = Math.round(cur.relative_humidity_2m);
        msg.DewPoint = Math.round(cur.dew_point_2m);
        msg.UseDewPoint = localStorage.getItem('useDewPoint') === '1' ? 1 : 0;
        msg.Wind = Math.round(cur.wind_speed_10m);
        msg.WindDir = degToCompass(cur.wind_direction_10m || 0);
        msg.Condition = mapWeatherCode(cur.weather_code);
        if (daily.temperature_2m_max && daily.temperature_2m_max.length) {
          msg.High = Math.round(daily.temperature_2m_max[0]);
          msg.Low = Math.round(daily.temperature_2m_min[0]);
        }
        if (daily.sunrise && daily.sunrise.length) {
          msg.Sunrise = fmtTime12(daily.sunrise[0]);
          msg.Sunset = fmtTime12(daily.sunset[0]);
        }
        var dailyMax = (daily.uv_index_max && daily.uv_index_max.length)
                       ? daily.uv_index_max[0] : null;
        if (typeof cur.uv_index === 'number') {
          msg.UV = Math.round(cur.uv_index);
        } else if (dailyMax !== null) {
          msg.UV = Math.round(dailyMax);
        }
        if (dailyMax !== null) {
          msg.UVMax = Math.round(dailyMax);
        }
        if (aqd && aqd.current) {
          msg.AQI = Math.round(aqd.current.us_aqi || 0);
        }
        msg.Units = units === 'metric' ? 1 : 0;
        msg.LastUpdated = Math.floor(Date.now() / 1000);

        // Index of the current hour in the hourly arrays, so "+1h" is
        // truly the next hour and not midnight.
        var p = hourly.precipitation_probability || [];
        var times = hourly.time || [];
        var startIdx = 0;
        if (times.length) {
          var now = new Date();
          var pad = function(n) { return n < 10 ? '0' + n : '' + n; };
          var nowKey = now.getFullYear() + '-' + pad(now.getMonth() + 1) +
                       '-' + pad(now.getDate()) + 'T' + pad(now.getHours()) + ':00';
          for (var k = 0; k < times.length; k++) {
            if (times[k] === nowKey) { startIdx = k; break; }
          }
        }

        // Rain alert: first hour (now..+6h) with a measurable amount —
        // round(amount*10) > 0, the app's droplet metric.
        var hPrcp = hourly.precipitation || [];
        var alert = -1;
        for (var j = 0; j <= 6; j++) {
          var amt = Math.round((hPrcp[startIdx + j] || 0) * 10);
          if (amt > 0) { alert = j === 0 ? 15 : j * 60; break; }
        }
        msg.RainAlertMinutes = alert;

        // Next 6 hours (+1h..+6h).
        var temps = hourly.temperature_2m || [];
        var codes = hourly.weather_code || [];
        for (var hi = 1; hi <= 6; hi++) {
          var idx = startIdx + hi;
          var hourLabel = '';
          if (times[idx]) {
            var hh = parseInt(times[idx].split('T')[1].split(':')[0], 10);
            if (use24h()) {
              hourLabel = String(hh);
            } else {
              var ampm = hh >= 12 ? 'PM' : 'AM';
              hh = hh % 12; if (hh === 0) hh = 12;
              hourLabel = hh + ' ' + ampm;
            }
          }
          msg['Hour' + hi + 'Label'] = hourLabel;
          msg['Hour' + hi + 'Temp']  = Math.round(temps[idx] || 0);
          msg['Hour' + hi + 'Cond']  = mapWeatherCode(codes[idx] || 0);
          msg['Hour' + hi + 'Pop']   = Math.round(p[idx] || 0);
        }

        // Week ahead (today + next 4 days).
        var dayCodes = daily.weather_code || [];
        var dayHigh  = daily.temperature_2m_max || [];
        var dayLow   = daily.temperature_2m_min || [];
        var dayTimes = daily.time || [];
        var dayNames = ['SUN','MON','TUE','WED','THU','FRI','SAT'];
        for (var di = 0; di < 5; di++) {
          var lbl = '';
          if (dayTimes[di]) {
            var dt = new Date(dayTimes[di] + 'T00:00');
            lbl = dayNames[dt.getDay()];
          }
          msg['Day' + di + 'Label'] = lbl;
          msg['Day' + di + 'High']  = Math.round(dayHigh[di] || 0);
          msg['Day' + di + 'Low']   = Math.round(dayLow[di]  || 0);
          msg['Day' + di + 'Cond']  = mapWeatherCode(dayCodes[di] || 0);
        }

        // Moon phase computed locally.
        var moon = computeMoonPhase(new Date());
        msg.MoonPhase = moon.phase;
        msg.MoonIllum = moon.illum;
        msg.MoonName1 = moon.name1;
        msg.MoonName2 = moon.name2;
      } catch (e) {
        console.log('parse err: ' + e.message);
        fetchDone();
        return;
      }
      Pebble.sendAppMessage(msg,
        function() {
          console.log('weather sent');
          localStorage.setItem('lastFetchAt', String(Date.now()));
          trackPing(lat, lon); // anonymous once-per-day active-user ping
          fetchDone();
        },
        function(e) {
          console.log('send fail: ' + JSON.stringify(e));
          fetchDone();
        }
      );
    });
  });
  });
}

// Fetch dedupe: `ready` and the C side's 750ms sentinel can both fire at
// launch; the in-flight guard absorbs whichever arrives second, and
// lastFetchAt lets `ready` skip when data is fresh (the app's pattern).
var FETCH_FRESH_MS = 15 * 60 * 1000;      // matches comm.c LAUNCH_REFRESH_SECS
var FETCH_IN_FLIGHT_MS = 20000;           // xhr timeout is 15s; self-heals
var fetchStartedAt = 0;

function fetchDone() { fetchStartedAt = 0; }

function maybeInitialFetch() {
  var last = parseInt(localStorage.getItem('lastFetchAt') || '0', 10);
  var age = Date.now() - last;
  if (last > 0 && age < FETCH_FRESH_MS) {
    console.log('ready: last fetch ' + Math.round(age / 1000) + 's ago, skipping');
    return;
  }
  locateAndFetch();
}

function locateAndFetch() {
  if (Date.now() - fetchStartedAt < FETCH_IN_FLIGHT_MS) {
    console.log('fetch already in flight, skipping');
    return;
  }
  fetchStartedAt = Date.now();
  var override = localStorage.getItem('locationOverride');
  if (override) {
    var parts = override.split(',');
    if (parts.length === 2) {
      fetchWeather(parseFloat(parts[0]), parseFloat(parts[1]));
      return;
    }
  }
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      fetchWeather(pos.coords.latitude, pos.coords.longitude);
    },
    function(err) {
      console.log('geo err: ' + err.message + ' — using fallback');
      fetchWeather(37.7749, -122.4194);
    },
    { timeout: 15000, maximumAge: 600000 }
  );
}

// Re-push the saved Clay settings on every PKJS launch. A config save is ONE
// AppMessage with no queue behind it: if it lands while the watch inbox is
// busy (a weather push mid-arrival, the face mid-restart, a BT hiccup) it is
// dropped silently, and the two sides then disagree forever — the config page
// shows the new value (Clay stored it phone-side before sending), the watch
// keeps the old one, and nothing ever reconciles. A watchface restarts every
// time the user visits an app and comes back, so re-sending the stored dict
// on 'ready' heals any divergence within one face launch. The watch side is
// idempotent by design: night mode re-asserts its override after any theme
// write, face_state_apply_mode reconciles gesture mode on every payload, and
// slot de-duplication happens at draw time.
function pushStoredSettings(done) {
  var stored = null;
  try { stored = localStorage.getItem('clay-settings'); } catch (err) {}
  if (!stored) { done(); return; }
  var msg;
  try {
    // getSettings accepts the stored flat dict as a response and returns the
    // numeric-keyed AppMessage dict (same path a real save takes).
    msg = clay.getSettings(stored);
  } catch (err) {
    console.log('stored clay-settings unreadable, skipping re-push: ' + err);
    done(); return;
  }
  Pebble.sendAppMessage(msg, function() { done(); }, function() {
    console.log('settings re-push failed, retrying once');
    setTimeout(function() {
      Pebble.sendAppMessage(msg, function() { done(); }, function() { done(); });
    }, 2000);
  });
}

Pebble.addEventListener('ready', function() {
  console.log('TouchyWeather Face PKJS ready');
  // Settings first, weather second — back-to-back sends race on the watch
  // (the second arrives while comm.c is still parsing the first and is
  // dropped), so the fetch waits for the config send to resolve.
  pushStoredSettings(function() { maybeInitialFetch(); });
});

Pebble.addEventListener('appmessage', function(e) {
  var p = (e && e.payload) || {};
  // The watch reports its system clock style with each refresh request so
  // the "Match watch" time format can follow it.
  if (p.ClockIs24h !== undefined) {
    localStorage.setItem('clockIs24h', p.ClockIs24h ? '1' : '0');
  }
  // Only fetch when explicitly requested via the LastUpdated sentinel;
  // config messages must not trigger a fetch.
  if (p.LastUpdated !== undefined) {
    console.log('appmessage: LastUpdated sentinel, fetching weather');
    locateAndFetch();
  }
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

// localStorage keys baked into the weather payload phone-side; only a
// change to one of these needs a refetch after a Clay save.
function weatherRelevantSnapshot() {
  return [
    localStorage.getItem('units'),
    localStorage.getItem('useDewPoint'),
    localStorage.getItem('timeFormat'),
    localStorage.getItem('locationOverride')
  ].join('|');
}

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;
  var beforeSave = weatherRelevantSnapshot();
  var dict = clay.getSettings(e.response, false);
  if (dict.Units !== undefined) {
    // Clay radiogroup values come back as strings ("0"/"1"); coerce first.
    localStorage.setItem('units',
      parseInt(dict.Units.value, 10) === 1 ? 'metric' : 'imperial');
  }
  if (dict.UseDewPoint !== undefined) {
    localStorage.setItem('useDewPoint', dict.UseDewPoint.value ? '1' : '0');
  }
  if (dict.TimeFormat !== undefined) {
    localStorage.setItem('timeFormat',
      String(parseInt(dict.TimeFormat.value, 10) || 0));
  }
  if (dict.LocationOverride !== undefined && dict.LocationOverride.value) {
    localStorage.setItem('locationOverride', dict.LocationOverride.value);
  } else {
    localStorage.removeItem('locationOverride');
  }
  var needsFetch = (weatherRelevantSnapshot() !== beforeSave);
  function afterSave() {
    if (needsFetch) {
      locateAndFetch();
    } else {
      console.log('config saved, no weather-relevant change, skipping fetch');
    }
  }
  // Retry a failed save send. Without this a save that catches the watch at
  // a busy moment is lost silently and the user's toggle "doesn't work" (the
  // ready-time re-push above would still heal it, but only on the next face
  // launch — retrying here makes the save itself land almost always).
  var msg = clay.getSettings(e.response);
  var attempts = 0;
  function trySend() {
    attempts++;
    Pebble.sendAppMessage(msg, afterSave, function() {
      if (attempts < 3) {
        console.log('config send failed (attempt ' + attempts + '), retrying');
        setTimeout(trySend, 1500);
      } else {
        console.log('config send failed after ' + attempts + ' attempts');
        afterSave();
      }
    });
  }
  trySend();
});
