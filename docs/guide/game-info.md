# Game info and art

Lattis can fill in a game's description, release year and rating from [IGDB](https://www.igdb.com), and find its art (hero, cover, logo and icon) on [SteamGridDB](https://www.steamgriddb.com).

Nothing is sent anywhere until you ask, or new games come in. Only titles, platforms and the services' own game ids are sent, never your file names or paths.

## Getting the keys

Both services are free, but each needs your own key. Enter them in Settings → Metadata (or the setup guide's Game info step). Each has a "How to get these" link and a **Test** button.

- **IGDB** belongs to Twitch, so its keys come from a Twitch developer app. Log in at [dev.twitch.tv](https://dev.twitch.tv/console/apps), register an application with any name, OAuth redirect URL `http://localhost` and client type Confidential, then copy its **Client ID** and a new **Client secret**.
- **SteamGridDB**: any SteamGridDB account can make an API key, at [steamgriddb.com/profile/preferences/api](https://www.steamgriddb.com/profile/preferences/api).

The keys are kept in `credentials.toml` in your [data folder](data-folder.md), readable only by you. A library backup includes them unless you uncheck it.

## New games are looked up by themselves

With the keys set, new ROMs from a scan, newly imported Steam games and shortcuts brought in from Steam are looked up as they arrive. When IGDB has a clear match, the game gets its description, year and rating, and SteamGridDB's top image for each kind of art it's missing. Games without a clear match are left as they are, and a message offers **Review**, which opens [Look up missing info](#look-up-missing-info).

The rating is what IGDB's members give the game, out of five stars to a tenth of a star (4.2 fills the fifth star a fifth of the way). A game with few ratings is pulled toward a middling score, and one with fewer than 20 gets none. Critics' scores aren't counted. Your own ratings stay in half stars. A rating from IGDB is marked "from IGDB" beside Rating in the game editor until you change it.

Your tags that fit a matched game are added too, never removed.

Turn this off with **Look up new games** on Settings → Metadata. Games you add with + aren't looked up by themselves, since you may want to fix the title first; use the dialog's "Look up in IGDB".

## Looking up one game

In a game's Properties, or the + dialog, click **Look up in IGDB** beside the title. It searches for the title as it's typed, on the game's platform, so fix a poor title first.

- **A clear match** fills in the fields you haven't touched, each marked "from IGDB". The row under the heading names the game it found, with **Not this game?**, **Detailed lookup**, **Choose art** and **Undo lookup**.
- **Several possible matches**: pick the right one from up to five, or **None of these**, or **Not on IGDB** (so bulk lookups skip it from then on).
- **Fields that already had something** are left alone and listed ("Description differs from what you have"), with **Replace these too**.
- **Tags that fit** are added, each with a dot until you keep or remove it.

Nothing is saved until you click Save.

### Detailed lookup

For a game IGDB lists under another name or platform: a recomp or a port, a fan translation, a file name far from the real title. **Detailed lookup** searches any name, on the game's platforms, every platform, or one you pick from about 45. It shows up to 50 results, with each one's cover, year, platforms and description. IGDB's search only finds whole words; when it finds nothing, the text is searched for anywhere in the names instead, so part of a word works too. **Use this game** applies it; **Cancel**, the X in the corner or Escape closes it without changes.

When the name you searched isn't the game's title, the title is kept: a Linux recomp named "Dusklight", found as Twilight Princess, keeps "Dusklight" and gets Twilight Princess's year, description and rating. **Replace it too** takes IGDB's title as well.

## Choosing art

Art is never picked for you in a game's Properties. **Browse SteamGridDB** (by the Images heading) or **Choose art** (after a lookup) opens the art browser: a tab for each kind (hero, cover, logo, icon), with SteamGridDB's images, a Style filter and **Load more**. Pick at most one per tab, then **Use selected**. **Cancel**, the X in the corner or Escape closes it without changes; in Look up missing info, where picks count as you make them, the X closes it like **Done**.

You can also use your own images: click a kind of art's preview to choose any image file. The × in its corner, shown on hover, removes it.

## Look up missing info

**Look up missing info** (Settings → Metadata, the app menu, or Ctrl+Shift+L) fills in many games at once. It lists the games without a description, year, rating or a kind of art, and says what each is missing.

1. **Pick the games.** The sidebar on the left chooses which games are listed, and the list on the right has a box to search titles. Check the games to look up (the box at the top of the list checks or unchecks them all), and click **Look up N**.
2. **Review.** Results come in one game at a time; **Stop** keeps what's come in so far. Click a result's row, or the arrow beside its badge, to fold its details away; **Collapse all** and **Expand all** do it for every result. Apply N counts folded results too.
3. **Apply.** Nothing is written until **Apply N**. Only empty fields and missing art are filled, so anything you edited meanwhile wins. **Back to the list** returns to the games you picked.

### Picking the games

The sidebar's sections narrow the list, and each choice shows how many games it would list:

- **Missing info | All games** switches between the games missing something and every game (for their tags).
- **Missing** has a box for each of Description, Year, Rating and Art. Uncheck one and games that lack only that aren't listed; its chip in the **Missing** column is dimmed.
- **Platform** lists one platform, or all of them.
- **Tags** opens a list of your tags with a box to search them. Check tags there to list the games that have any of them; the checked ones stay in the sidebar, where a click on one unchecks it.
- **Games not on IGDB** lists the games you've marked as not on IGDB, which are left out otherwise.
- **Clear filters**, at the bottom, undoes all of it.

Two options in the bottom bar change what's looked up:

- **Replace existing values** also offers IGDB's description, year and rating where they differ from yours ("Year 2020 was 2018"), unchecked, for you to pick. A rating that already matches IGDB's is offered too (as "same, mark as from IGDB") until it is marked as coming from IGDB. With a SteamGridDB key and Art among what's looked for, SteamGridDB's top image is offered for the kinds of art a game already has as well, also unchecked. The sidebar's Missing section becomes **Look for**, since every game can then get them. Titles are never changed in bulk.
- **Suggest tags** is described [below](#tags-from-igdb).

### Reviewing the results

The review sidebar shows how far the lookup has got, and **Show** narrows the list by result, with a count for each: All results, Matched, Needs a pick, Not found, Couldn't look up and Not on IGDB. Results stay in the order they came in. Under **For every match**, a box for each kind of change checks or unchecks it in every result at once, with a count of how many there are: under **Replace existing**, Description, Year and Rating (IGDB's value in place of yours) and Art (SteamGridDB's in place of yours); under **Add**, Tags and Art. A dash means some are checked. The heading's own box does its whole group, so **Replace existing** is the old "replace all". A row shows only when the results offer it. Under **View**, **Choose art for all** steps through every game's art, and **Collapse all** folds the results to one line each.

- **Matched:** the game's row opens to show IGDB's match, with its cover. **Info** has what would be filled in, as checked chips; **Tags** has the tags that fit (see below); **Art** shows the game's art put together the way its page will look: the hero, with the logo and icon on it and the theme's darkening behind the logo, and the cover beside it. Each missing kind gets SteamGridDB's top image; kinds the game already has show its own art. Uncheck a chip to leave it out, or one of the Hero, Logo, Icon and Cover boxes under the art; an unchecked picture is dimmed. With **Replace existing values**, a kind the game has reads "replaces yours": its box starts unchecked and the preview shows your picture, and checking it swaps in SteamGridDB's. Click a picture to choose another image from SteamGridDB (which checks it). A kind SteamGridDB has nothing for reads "none found", and one the game keeps reads "yours". Art you change after the lookup is never replaced by Apply.
- **Which game?:** pick one of the candidates, or open **Detailed lookup** to search another name or platform, or choose **Not on IGDB**.
- **Not found:** **Detailed lookup**, or **Not on IGDB** to skip the game in later lookups. A game marked that way shows **Undo**, which puts it back as it was. With a SteamGridDB key, a game IGDB doesn't know (a ROM hack, say) still gets art when SteamGridDB has a game of that exact name: its row shows the art to review, and Apply saves it, whether the game is marked Not on IGDB or not.
- **Couldn't look up:** the error, and **Retry**.

### Tags from IGDB

With **Suggest tags** on (beside Look up N), each match also shows your tags that fit the game, as checked chips. Uncheck the ones you don't want. Tags are only ever added, never removed.

**Suggest tags from IGDB**, on Settings → Library, opens this window with every game listed and checked, to tag your whole library in one go.

Each default tag knows which IGDB genres, themes and keywords it stands for. Tags you create yourself don't, so they're only suggested if they share a default tag's name.
