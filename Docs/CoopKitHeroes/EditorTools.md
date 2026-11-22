# CoopKitHeroes: editor tools

Step-by-step pictures of the editor tools of CoopKitHeroes. You do everything with the mouse; the only typing is a name for a new asset. Your editor may look slightly different from the pictures.

The pictures are in `Docs/Images`.

## Show the plugin's content

The plugin's own assets (examples, demo maps) live in the plugin folder. To see them in the Content Browser, click **Settings** in the Content Browser and turn on **Show Plugin Content**.

![Content Browser Settings menu with Show Plugin Content](Images/ContentBrowserSettings.png)

## Create a hero

1. In the Content Browser, open the folder for the new asset.
2. Click **Add**, point to **CoopKit**, then click **Hero**.
3. Enter a name for the asset.

![Add menu, CoopKit submenu, Hero entry](Images/CreateAsset.png)

## Open the CoopKit Hub

The hub shows the entities of every CoopKit plugin in your project in one window. CoopKitHeroes adds the category **Hero**.

1. On the main menu, click **Tools**, then click **CoopKit Hub**.

![Tools menu with the CoopKit Hub entry](Images/ToolsMenu.png)

2. The hub opens on the categories. Each entity type has a button that shows the number of its entities and the name of the plugin that adds it.

![CoopKit Hub with the categories, Hero highlighted](Images/Hub.png)

## Find a hero

1. Click the category **Hero**. The left side now lists only this category. It also has a search box, the name box and **New Hero**. Click **< Back** to return to the categories.

![Hero category open, Back highlighted](Images/HubPickType.png)

2. Click a hero in the list. The right side shows its name, type, asset path and id. **Open** opens its card in a separate window (a double click in the list does the same).

![Hero entities, one selected](Images/HubEntities.png)

3. **Where Used** shows the assets that use the selected hero.

![Where Used result](Images/HubWhereUsed.png)

4. **Check Project** checks every entity of every type: data errors and duplicate ids. The result appears in the text box below the buttons.

![Check Project result](Images/HubCheckProject.png)

## Make a new hero with the wizard

1. In the **Hero** category, click the name box and enter a name (letters, digits, underscores). The button **New Hero** turns on.

![Name entered, New Hero button highlighted](Images/HubNewName.png)

2. Click **New Hero**. The wizard opens. Set the display name, the role and the mesh, or pick a hero to copy from, and tick what to create: the wizard makes the hero's own ability, effect and skin material in its folder. The mesh and the Animation Blueprint stay shared. Click **Create**.

![New Hero wizard](Images/NewWizard.png)

3. The card of the new hero opens. The new hero appears in the hub's list and is selected. The text box below the buttons says "Created" and shows the full asset path. The new assets are not saved yet: click **Save All** in the Content Browser to keep them.

![Hub after the wizard](Images/HubCreated.png)

## The hero card

The card shows everything about one hero. Open it from the hub (**Open**) or double-click the asset in the Content Browser.

![Hero card](Images/Card.png)

- **Properties**: the data of the hero.

![Properties section](Images/CardProperties.png)

- **Preview**: the hero in 3D; the **Skin** list shows each skin.

![Preview section](Images/CardPreview.png)

- **Parts**: every asset the hero uses. **own** parts lie in the hero's folder and are renamed, duplicated and deleted with it; **shared** parts are used by other assets too and are never touched. **Open** opens a part in its editor, **Create** makes a new own part.

![Parts section, Replace highlighted](Images/CardParts.png)

- To use another asset for a part, click **Replace** and click the asset in the list.

![Replace list open](Images/CardReplacePart.png)

- **Summary**: what the data means in practice.

![Summary section](Images/CardSummary.png)

- **Validation**: problems of the data, refreshed on every change. **Validate** checks again.

![Validation without problems](Images/CardValidation.png)

When a value does not work, Validation shows the problem in red right away, for example an empty **Hero Id**.

![Validation with an error](Images/CardValidationError.png)

## Hero Roster and Hero Comparison

**Tools > Hero Roster** lists every hero of the project. **Duplicate**, **Rename** and **Delete** work on a hero with all its own parts; shared parts are never touched. **Save All** saves the changes.

![Hero Roster](Images/HeroRoster.png)

**Tools > Hero Comparison** shows the heroes side by side, for balancing.

![Hero Comparison](Images/HeroComparison.png)

## Play the demo with the mouse

1. Show the plugin's content (see above), double-click `/CoopKitHeroes/Maps/L_HeroesDemo`, then click **Play** in the toolbar.

![Play button in the toolbar](Images/PlayButton.png)

2. The panel in the top left corner lists the heroes of the roster with **Play** and a skin button, and the players with their heroes. You play the Knight. Click **Play with 2 windows**. Play restarts with two windows. Your Play settings stay as they were.

![One window with Play with 2 windows](Images/DemoOneWindow.png)

3. In window 2, click **Play** on the *Knight* row. Player 1 plays the Knight, so the server refuses.

![The server refused: Taken](Images/DemoRefused.png)

4. In window 2, click **Play** on the *Mage* row. Both windows list Player 2 as the Mage.

![Player 2 plays the Mage](Images/DemoMage.png)

5. In window 1, click **Look at** on the *Player 2* row, then a skin button. Both windows show the new skin.

![The Knight in the Ember skin](Images/DemoSkin.png)

## Install the AI agent skills

The skills let an AI coding agent extend the plugin with you (see *AI agent skills* in the README).

1. On the main menu, click **Tools**, point to **CoopKit**, then click **Install AI Agent Skills**. The CoopKit Hub has the same button.

![Tools, CoopKit, Install AI Agent Skills](Images/ToolsMenuCoopKit.png)

![Install AI Agent Skills button in the hub](Images/HubSkillsButton.png)

2. A message lists the skill folders that were copied into your project. Click **OK**. If some folders could not be copied, the message shows a warning icon and lists them.

![Skills installed](Images/SkillsInstalled.png)

3. If the skills are already in the project, the plugin asks first. **Yes** replaces them and removes any change you made to their files; **No** keeps them.

![Replace the installed skills?](Images/SkillsReplace.png)
