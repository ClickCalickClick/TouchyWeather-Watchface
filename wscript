#
# This file is the default set of rules to compile a Pebble application.
#
# Feel free to customize this to your needs.
#
import os.path
import json
import re

top = '.'
out = 'build'

# The card cannot scroll (a watch face gets no buttons and no touch), so the
# notes have to fit one screen. Chalk (180 round) is the binding constraint and
# affords roughly three short lines. These are warnings, not errors: an over-long
# entry still builds, it just gets demoted/ellipsised at draw time.
NOTES_MAX_BULLETS = 4
NOTES_MAX_BULLET_CHARS = 34


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    """
    This method is used to configure your build. ctx.load(`pebble_sdk`) automatically configures
    a build for each valid platform in `targetPlatforms`. Platform-specific configuration: add your
    change after calling ctx.load('pebble_sdk') and make sure to set the correct environment first.
    Universal configuration: add your change prior to calling ctx.load('pebble_sdk').
    """
    ctx.load('pebble_sdk')


def ensure_pkjs_secrets(ctx):
    """Create src/pkjs/secrets.js from secrets.js.example when missing.

    secrets.js holds the real proxy key and is gitignored so it never lands
    in the public repo; this hook lets a fresh clone compile out of the box
    (with an empty key — proxy-backed features just 401 until one is added).
    """
    if ctx.path.find_node('src/pkjs/secrets.js') is None:
        example = ctx.path.find_node('src/pkjs/secrets.js.example')
        if example is not None:
            ctx.path.make_node('src/pkjs/secrets.js').write(example.read())


def _c_escape(s):
    """Escape a Python string for embedding in a C string literal."""
    return (s.replace('\\', '\\\\')
             .replace('"', '\\"')
             .replace('\n', '\\n')
             .replace('\r', '')
             .replace('\t', ' '))


def generate_version_header(ctx):
    """Generate src/c/version_gen.h from CHANGELOG.md's top entry.

    Ported from the TouchyWeather watchapp, which owns the same "New on the
    horizon" card. The newest release is the first `## x.y.z` block; its version
    drives the show-once trigger and its bullet lines become the card's body.
    Runs on every build, so the only per-release action is editing CHANGELOG.md
    (+ bumping package.json).

    Divergence from the app: we also emit APP_HAS_UPDATE_NOTES. The face only
    shows the card when the release actually said something, and deciding that
    here — where we know whether real bullets were parsed — beats string-
    comparing a sentinel at runtime.
    """
    from waflib import Logs

    changelog = ctx.path.find_node('CHANGELOG.md')
    code, label, notes, has_notes = 0, '0.0.0', '', 0
    if changelog:
        text = changelog.read()
        # First "## x.y.z" header and everything up to the next "## ".
        m = re.search(r'(?m)^##\s+(\d+)\.(\d+)\.(\d+)[^\n]*\n(.*?)(?=^##\s|\Z)',
                      text, re.S)
        if m:
            major, minor, patch = int(m.group(1)), int(m.group(2)), int(m.group(3))
            code = major * 10000 + minor * 100 + patch
            label = '%d.%d.%d' % (major, minor, patch)
            bullets = []
            for line in m.group(4).splitlines():
                line = line.strip()
                if line.startswith('-'):
                    bullets.append(line[1:].strip())
            notes = '\n'.join(bullets)
            has_notes = 1 if bullets else 0

            # The card has one screen and no way to scroll it. Warn rather than
            # fail: the drawer's fallback ladder demotes the font and elides the
            # tail, so an over-long entry degrades instead of breaking.
            if len(bullets) > NOTES_MAX_BULLETS:
                Logs.warn('version_gen: %d bullets in %s; only ~%d fit the small '
                          'screen classes, the rest collapse into "+N more"'
                          % (len(bullets), label, NOTES_MAX_BULLETS))
            for b in bullets:
                if len(b) > NOTES_MAX_BULLET_CHARS:
                    Logs.warn('version_gen: bullet is %d chars (soft limit %d), '
                              'will wrap or ellipsise on chalk: "%s"'
                              % (len(b), NOTES_MAX_BULLET_CHARS, b))
        else:
            Logs.warn('version_gen: no "## x.y.z" entry found in CHANGELOG.md')

        # Guarantee versions only ever go up: the top entry must be strictly
        # greater than the previous one. Fails the build otherwise so a
        # non-increasing version can never be shipped.
        vers = re.findall(r'(?m)^##\s+(\d+)\.(\d+)\.(\d+)', text)
        if len(vers) >= 2:
            newest = tuple(int(x) for x in vers[0])
            prev = tuple(int(x) for x in vers[1])
            newest_code = newest[0] * 10000 + newest[1] * 100 + newest[2]
            prev_code = prev[0] * 10000 + prev[1] * 100 + prev[2]
            if newest_code <= prev_code:
                ctx.fatal('version_gen: CHANGELOG top version %d.%d.%d must be '
                          'greater than the previous entry %d.%d.%d'
                          % (newest + prev))
    else:
        Logs.warn('version_gen: no CHANGELOG.md; the update-notes card is disabled')

    # Warn (non-fatal) if the store version drifts from the changelog top.
    pkg = ctx.path.find_node('package.json')
    if pkg:
        try:
            pkg_version = json.loads(pkg.read()).get('version', '')
            if pkg_version and pkg_version != label:
                Logs.warn('version_gen: package.json version (%s) != CHANGELOG top (%s)'
                          % (pkg_version, label))
        except ValueError:
            pass

    ctx.path.make_node('src/c/version_gen.h').write(
        '#pragma once\n'
        '//\n// AUTOGENERATED BY BUILD FROM CHANGELOG.md\n'
        '// DO NOT EDIT - CHANGES WILL BE OVERWRITTEN\n//\n'
        '#define APP_VERSION_CODE       %d\n' % code +
        '#define APP_VERSION_LABEL      "%s"\n' % _c_escape(label) +
        '#define APP_HAS_UPDATE_NOTES   %d\n' % has_notes +
        '#define APP_UPDATE_NOTES       "%s"\n' % _c_escape(notes)
    )


def build(ctx):
    ctx.load('pebble_sdk')

    # Materialize the generated headers before the C/JS globs run.
    generate_version_header(ctx)
    ensure_pkjs_secrets(ctx)

    build_worker = os.path.exists('worker_src')
    binaries = []

    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'), target=app_elf, bin_type='app')

        if build_worker:
            worker_elf = '{}/pebble-worker.elf'.format(ctx.env.BUILD_DIR)
            binaries.append({'platform': platform, 'app_elf': app_elf, 'worker_elf': worker_elf})
            ctx.pbl_build(source=ctx.path.ant_glob('worker_src/c/**/*.c'),
                          target=worker_elf,
                          bin_type='worker')
        else:
            binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js',
                                         'src/pkjs/**/*.json',
                                         'src/common/**/*.js']),
                   js_entry_file='src/pkjs/index.js')
