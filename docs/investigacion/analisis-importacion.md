# Import Analysis

Scope: analysis of the folders `Holyrics 2` and `Himnario NUEVA VERSION PC 3`.

Goal of this document:
- Identify what data exists.
- Separate importable material from proprietary containers.
- Estimate the work needed to build importers for the new OBS fork.

## 1. Directory Structure

### `Holyrics 2`

Top-level structure observed:

- `Holyrics 2/Holyrics`
  - `data`
    - `bible_books`
    - `dict`
    - `fonts`
    - `help`
    - `i18n`
    - `theme`
  - `files`
    - `Bible LG`
    - `settings`
- `Holyrics 2/com/limagiran/holyrics`
- `Holyrics 2/fix/com/limagiran`
- `Holyrics 2/lib`
- `Holyrics 2/logs/holyrics`
- package files at root:
  - `Holyrics.jar`
  - `Holyrics_backup.jar`
  - `Holyrics-plugin.jar`

Relevant counts:
- Directories: 27
- Files: 200

### `Himnario NUEVA VERSION PC 3`

Top-level structure observed:

- `Adobe AIR/Versions/1.0`
- `HimnarioAdventista.app/Contents`
- `Letra`
- `META-INF/AIR`
- `Musica/Cantado`
- `Musica/Instrumental`
- `Resources`
- standalone files:
- `Himnario Adventista.exe`
- `autorun.inf`
- `himnario.ico`
- `mimetype`
- `Recursos Biblicos.url`

The `Letra` tree contains one folder per hymn, named like:

- `001 - Cantad alegres al Senor`
- `002 - Da gloria al Senor`
- ...
- `614 - Amenes`

Relevant counts:
- Hymn folders under `Letra`: 615
- Files under `Letra`: 4873 JPG slides plus 166 `Thumbs.db`
- `Musica/Cantado`: 623 MP3 files
- `Musica/Instrumental`: 622 MP3 files
- Total directories in the folder tree: 745
- Total files in the folder tree: 6717

## 2. File Types Used

Observed extension inventory across both folders:

- `jpg`: 4903
- `mp3`: 1245
- `db`: 167
- `dat`: 60
- `jar`: 29
- `js`: 26
- `xml`: 20
- `png`: 250
- `nib`: 48
- `strings`: 48
- `plist`: 6
- `css`: 10
- `svg`: 6
- `woff`: 6
- `ttf`: 17
- `eot`: 6
- `gif`: 13
- `zip`: 7
- `exe`: 2
- `hly`: 3
- `words`: 2
- `bib`: 3
- `dll`: 5
- `dylib`: 1
- `icns`: 1
- `ico`: 1
- `url`: 1
- `inf`: 1
- `txt`: 1
- `rsrc`: 1
- `html`: 3
- `java`: 1
- `class`: 4
- `lzma`: 1
- `bmp`: 1
- `6`: 1
- `[no ext]`: 13

Notes:
- Many `.db` entries are Windows thumbnail caches (`Thumbs.db`), not real application databases.
- `Holyrics` uses compiled Java artifacts (`.jar`, `.class`) and custom serialized data (`.dat`, `.hly`).
- `Himnario` uses Adobe AIR + HTML/JS/CSS + image and audio assets.

## 3. Databases Found

### Real application database candidates

1. `Holyrics 2/Holyrics/files/database.db`
   - SQLite file present.
   - File size is 0 bytes.
   - `sqlite3` shows no schema.
   - Conclusion: it is a placeholder or empty database, not a usable content source.

### Non-database `.db` files

2. `Thumbs.db` files inside `Himnario NUEVA VERSION PC 3/Letra/*`
   - 166 files.
   - These are Windows Explorer preview caches, not app data.

## 4. SQLite Files Found

Only one SQLite-like file was found:

- `Holyrics 2/Holyrics/files/database.db`

Status:
- Empty file.
- No tables or schema.
- Not useful for direct import as-is.

## 5. JSON Found

No loose `.json` files were found in the root trees, but Holyrics contains a large JSON corpus inside ZIP archives:

- `Holyrics 2/Holyrics/data/bible_books/0000.zip`
- `Holyrics 2/Holyrics/data/bible_books/0001.zip`
- `Holyrics 2/Holyrics/data/bible_books/0002.zip`
- `Holyrics 2/Holyrics/data/bible_books/0003.zip`
- `Holyrics 2/Holyrics/data/bible_books/0004.zip`
- `Holyrics 2/Holyrics/data/bible_books/0005.zip`

Total JSON files inside these ZIPs:
- 1918 JSON files

Observed structure:
- top-level metadata such as `id`, `language`, `name`, `altName`
- `books` array with 66 Bible books

Example:
- `0000.zip` contains 1121 JSON files
- `0001.zip` contains 20 JSON files
- `0002.zip` contains 3 JSON files
- `0003.zip` contains 389 JSON files
- `0004.zip` contains 381 JSON files
- `0005.zip` contains 4 JSON files

These look like language-pack Bible datasets and are the most importable Bible source in the entire sample.

## 6. XML Found

Standalone XML files were found mainly in the Adobe AIR bundle of Himnario:

- `Himnario NUEVA VERSION PC 3/META-INF/AIR/application.xml`
- `Himnario NUEVA VERSION PC 3/META-INF/signatures.xml`
- `Himnario NUEVA VERSION PC 3/HimnarioAdventista.app/Contents/Resources/META-INF/AIR/application.xml`
- `Himnario NUEVA VERSION PC 3/HimnarioAdventista.app/Contents/Resources/META-INF/signatures.xml`
- several `ContentUIText.xml` files inside `Adobe AIR.framework`

Conclusion:
- Holyrics has no loose XML content files in the analyzed tree.
- Himnario uses XML for AIR packaging and localization resources, not for hymn catalog data.

## 7. Configuration Files

### Holyrics

- `Holyrics 2/Holyrics/files/settings`
- `Holyrics 2/Holyrics/files/set.hly`
- `Holyrics 2/Holyrics/files/themes.hly`
- `Holyrics 2/Holyrics/data/cf.hly`
- `Holyrics 2/Holyrics/logs/holyrics/settings.txt`
- `Holyrics 2/Holyrics/files/Bible LG/en_akjv.bib`
- `Holyrics 2/Holyrics/files/Bible LG/en_kjv.bib`
- `Holyrics 2/Holyrics/files/Bible LG/es_rv.bib`

### Himnario

- `Himnario NUEVA VERSION PC 3/META-INF/AIR/application.xml`
- `Himnario NUEVA VERSION PC 3/META-INF/signatures.xml`
- `Himnario NUEVA VERSION PC 3/autorun.inf`
- `Himnario NUEVA VERSION PC 3/mimetype`
- `Himnario NUEVA VERSION PC 3/Resources/js/script.js`
- `Himnario NUEVA VERSION PC 3/Resources/css/style.css`
- `Himnario NUEVA VERSION PC 3/Resources/css/base.css`
- `Himnario NUEVA VERSION PC 3/Resources/css/icons.css`
- `Himnario NUEVA VERSION PC 3/Resources/js/plugins.js`

## 8. Bibles Found

### Holyrics Bible sources

1. `Holyrics 2/Holyrics/files/Bible LG/en_akjv.bib`
2. `Holyrics 2/Holyrics/files/Bible LG/en_kjv.bib`
3. `Holyrics 2/Holyrics/files/Bible LG/es_rv.bib`

Observed file behavior:
- `file` identifies them as generic `data`, not text.
- They are likely serialized or proprietary Bible databases.
- They are not directly readable as plain text.

### Holyrics Bible book packs

The `bible_books` ZIP corpus contains many Bible language packs with 66-book structures.

Examples from the archive metadata:
- `aai.json` for Arifama-Miniafia
- `bhi.json` for Bhilali
- `bua.json` for Buriat
- `abn.json` for Abua
- `abr.json` for Abron

Implication:
- Holyrics already ships with a large multilingual Bible dataset that can likely be imported directly if the JSON format is honored.

## 9. Hymns Found

### Hymn catalog

The Himnario package contains 615 hymn folders in `Letra`.

The catalog is indexed in HTML and uses hymn numbers from `001` to `614`, with a special `000` folder and a final `614 - Amenes` entry.

### Contents per hymn

Typical hymn folder contents:
- many JPEG slides named sequentially (`01.jpg`, `02.jpg`, etc.)
- optional `Thumbs.db`

### Audio

Audio is split into two modes:
- `Musica/Cantado`
- `Musica/Instrumental`

Counts:
- 623 MP3 files in `Cantado`
- 622 MP3 files in `Instrumental`

Interpretation:
- one hymn may have multiple versions or extra variants
- some tracks are alternate endings, medleys, or versions with suffixes

### Special case

- `614 - Amenes` is a special entry and appears to have no active slide/timing payload in the HTML catalog.

## 10. Multimedia Resources Found

### Holyrics

- `Holyrics 2/Holyrics/data/theme/img/*`
  - background images
  - theme previews
  - video placeholder images and GIFs
- `Holyrics 2/Holyrics/data/help/img/*`
  - screenshots and help illustrations
- `Holyrics 2/Holyrics/data/fonts/*`
- `Holyrics 2/Holyrics/data/ffmpeg-lzma.zip`
- `Holyrics 2/Holyrics/data/VLC_mac.lzma`
- `Holyrics 2/Holyrics/data/dict/*.words`
- `Holyrics 2/Holyrics/data/i18n/*.dat`

### Himnario

- `Resources/css/img/*`
  - UI icons
  - theme icons
  - buttons
  - backgrounds
  - help images
- `Resources/icons/*`
  - application icons in multiple sizes
- `Resources/fonts/*`
  - custom web fonts
- `Letra/*/*.jpg`
  - hymn slide images
- `Musica/Cantado/*.mp3`
- `Musica/Instrumental/*.mp3`

### Packaging/media runtimes

- Adobe AIR runtime bundle
- macOS `.app` bundle
- Windows `.exe`

## 11. Possible Proprietary Formats

The following look proprietary or application-specific:

- `Holyrics 2/Holyrics/data/key_value_db.dat`
- `Holyrics 2/Holyrics/data/cf.hly`
- `Holyrics 2/Holyrics/files/set.hly`
- `Holyrics 2/Holyrics/files/themes.hly`
- `Holyrics 2/lib/pack*.dat`
- `Holyrics 2/Holyrics/data/i18n/*.dat`
- `Holyrics 2/Holyrics/data/dict/*.words`
- `Holyrics 2/Holyrics/files/Bible LG/*.bib`

Why:
- they are not readable as normal text
- `file` reports them as generic data
- some are ZIP containers or opaque serialized blobs

Holyrics `key_value_db.dat` is especially important:
- it is a ZIP archive with 206 entries
- entries are named like:
  - `content.worshipwide.WW....dat`
  - `jsc.action....dat`
  - `jsc.i18n....dat`
  - `jsc.module....dat`
  - `jsc.file_import.song.holyrics_json....dat`
- this suggests a key/value or resource store with embedded app logic and content records

## 12. How Hymns Sync With Images and Audio

This is clearly defined in `Himnario NUEVA VERSION PC 3/Resources/js/script.js`.

Main mechanism:

1. The catalog is embedded in `Resources/index.html`.
2. Each hymn entry stores metadata in DOM attributes such as:
   - `data-number`
   - `data-title`
   - `data-slides`
   - `data-times`
3. The JS code normalizes the title and builds paths from the hymn number/title.
4. Slide images are loaded from:
   - `Letra/<NNN - Title>/<slide>.jpg`
5. Audio is loaded from:
   - `Musica/<mode>/<NNN - Title>.mp3`
6. The `times` field provides the automatic slide timing offsets.
7. Playback advances by comparing the current audio position to those timing offsets.

Practical meaning:
- the synchronization is explicit, not inferred
- the app does not need manual scene creation per hymn
- a future importer can reconstruct hymn slide order and timings directly from the HTML

## 13. What Can Be Imported Automatically

### From Himnario

High confidence automatic import:
- hymn number
- hymn title
- slide count
- slide order
- slide images
- audio files
- automatic slide timings
- category/theme context if extracted from the catalog

Likely automatic with moderate effort:
- playlist structures, if related `.hapl` files exist outside the analyzed tree
- UI iconography and theme backgrounds for reference

### From Holyrics

High confidence automatic import:
- Bible language packs from `bible_books/*.zip`
- book/chapter/verse structure from JSON
- visible theme/help assets if used as reference material

Likely automatic if the format is decoded:
- `jsc.i18n.*.dat` localization resources
- some content entries from `key_value_db.dat`
- maybe songs or worship items if the `content.worshipwide.*` records are parsed

## 14. What Would Require Reverse Engineering

### Holyrics

Requires reverse engineering or format discovery:
- `key_value_db.dat`
- `cf.hly`
- `set.hly`
- `themes.hly`
- `pack*.dat`
- `i18n/*.dat`
- `dict/*.words`
- `Bible LG/*.bib`
- the meaning of `content.worshipwide.*` and `jsc.action.*` payloads

Reason:
- these are opaque binary or serialized formats
- there is no direct schema in the filesystem
- their exact structure is not documented in the analyzed sample

### Himnario

May require reverse engineering only if we want to extend the original data model:
- non-obvious playlist format, if present elsewhere
- hidden AIR-specific state or cache files
- exact normalization rules for title-to-filename generation in edge cases

But for the core hymn content, reverse engineering is not strictly necessary because the data is already explicit in HTML/JS and filesystem layout.

## Technical Conclusion

### Best import candidates

1. `Himnario` is the easiest high-value source.
   - hymns are already split into images and MP3s
   - timings are embedded in the catalog
   - a direct importer can be built without guessing the content model

2. `Holyrics` is most useful for Bible content and possibly song data.
   - the Bible book packs are already structured JSON inside ZIPs
   - the main app data is otherwise mostly opaque and will need deeper work

### Recommended import order

1. Import Himnario hymns, slides, and audio.
2. Import Holyrics Bible JSON packs.
3. Investigate Holyrics opaque content stores only after the first two are working.

### Bottom line

The user-facing presentation content is already present in both folders.
The main challenge is not content extraction, but decoding Holyrics proprietary stores and mapping both sources into the new native presentation model of the OBS fork.
