# opensweeper — Google Play store listing

Copy/paste these into the Play Console (**Grow → Store presence → Main store
listing**, plus **Store settings** for category). The images in this folder are
the shipped assets. `scripts/play_release.py` pushes the fields below and the
images from this folder, so edit them here rather than in the console.

The screenshots regenerate with `scripts/gen_store_screenshots.mjs`, and the
icon and feature graphic with `scripts/gen_icons.py`.

## Assets (this folder)

| File | Play field | Spec |
|------|-----------|------|
| `icon-512.png` | App icon | 512×512 PNG (32-bit) |
| `feature-graphic-1024x500.png` | Feature graphic | 1024×500 PNG/JPG |
| `screenshots/phone/` | Phone screenshots | 3× 1080×1920 PNG |
| `screenshots/phone-landscape/` | Phone screenshots (sideways) | 3× 1920×1080 PNG |
| `screenshots/tablet/` | 7-inch and 10-inch tablet screenshots | 3× 1600×2560 PNG |
| `screenshots/tablet-landscape/` | Tablet screenshots (sideways) | 3× 2560×1600 PNG |

The game plays in both orientations, so each slot has an upright and a sideways
set. Upload one orientation per slot, not a mixture.

## App name (≤30 chars)

```
opensweeper
```

## Short description (≤80 chars)

```
The classic mine-sweeping puzzle. Free, no ads, no tracking.
```

## Full description (≤4000 chars)

```
The classic mine-sweeping puzzle. Reveal every cell that is not a mine. Each number tells you how many mines touch that cell; use them to work out where the mines are and flag them.

No ads. No tracking. No accounts. No in-app purchases. opensweeper requests zero permissions and never touches the network. It's just the game.

HOW TO PLAY
• Tap a cell to reveal it. The first tap is always safe
• Press and hold to place a flag
• Tap a revealed number to clear its neighbours once its flags are placed
• Tap the face to start over

THREE DIFFICULTIES
• Beginner: 9 x 9, 10 mines
• Intermediate: 16 x 16, 40 mines
• Expert: 30 x 16, 99 mines
• Optional question-mark flags

FITS YOUR SCREEN
• Play upright or sideways, on a phone or a tablet
• Expert's wide grid stands upright on a phone held upright, so the cells stay as large as possible

BUILT RIGHT
• Fully offline — flights, waiting rooms, anywhere
• Tiny download, easy on your battery

FREE AND OPEN SOURCE
opensweeper is open source. Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/opensweeper
```

## Categorization (Store settings)

- **App or game:** Game
- **Category:** Puzzle
- **Tags:** puzzle, logic, brain games, classic
- **Email:** dan@danheskett.com
- **Website:** https://danheskett.com
- **Content rating:** Everyone (no objectionable content; IARC questionnaire —
  answer "no" to all violence/adult/gambling items)
- **Target audience:** 13-15, 16-17, 18 and over (not a children's app, so
  the Families policy does not apply)

## Data safety (Policy → App content)

- Data collected: **None**
- Data shared: **None**
- App has no `INTERNET` permission (verify in the manifest) → "no data
  transmitted off the device" is truthful.
- Privacy policy URL: **https://danheskett.com/app/privacy-policy/**

## Screenshots

Pushed via the Play API from the folders above. Captured from the web build --
the same C the Android app runs -- in a headless browser at each target size:

    npm i playwright-core
    make web
    node scripts/gen_store_screenshots.mjs --src build/web

That one command also refreshes the App Store sets under
`ios/app-store-assets/screenshots/`, so the two listings cannot drift apart.
Regenerate whenever the board, the chrome or the font changes.
