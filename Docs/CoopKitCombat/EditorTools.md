# CoopKitCombat: editor tools

Step-by-step pictures of the editor tools of CoopKitCombat. You do everything with the mouse; the only typing is a name for a new asset. Your editor may look slightly different from the pictures.

The pictures are in `Docs/Images`.

## Show the plugin's content

The plugin's own assets (examples, demo maps) live in the plugin folder. To see them in the Content Browser, click **Settings** in the Content Browser and turn on **Show Plugin Content**.

![Content Browser Settings menu with Show Plugin Content](Images/ContentBrowserSettings.png)

## Create a move set

1. In the Content Browser, open the folder for the new asset.
2. Click **Add**, point to **CoopKit**, then click **Combat Move Set**.
3. Enter a name for the asset.

![Add menu, CoopKit submenu, Combat Move Set entry](Images/CreateAsset.png)

## Open the CoopKit Hub

The hub shows the entities of every CoopKit plugin in your project in one window. CoopKitCombat adds the category **Move Set**.

1. On the main menu, click **Tools**, then click **CoopKit Hub**.

![Tools menu with the CoopKit Hub entry](Images/ToolsMenu.png)

2. The hub opens on the categories. Each entity type has a button that shows the number of its entities and the name of the plugin that adds it.

![CoopKit Hub with the categories, Move Set highlighted](Images/Hub.png)

## Find a move set

1. Click the category **Move Set**. The left side now lists only this category. It also has a search box, the name box and **New Move Set**. Click **< Back** to return to the categories.

![Move Set category open, Back highlighted](Images/HubPickType.png)

2. Click a move set in the list. The right side shows its name, type and asset path. **Open** opens its card in a separate window (a double click in the list does the same).

![Move Set entities, one selected](Images/HubEntities.png)

3. **Where Used** shows the assets that use the selected move set.

![Where Used result](Images/HubWhereUsed.png)

4. **Check Project** checks every entity of every type: data errors and duplicate ids. The result appears in the text box below the buttons.

![Check Project result](Images/HubCheckProject.png)

## Make a new move set with the wizard

1. In the **Move Set** category, click the name box and enter a name (letters, digits, underscores). The button **New Move Set** turns on.

![Name entered, New Move Set button highlighted](Images/HubNewName.png)

2. Click **New Move Set**. The wizard opens. Pick a display name, a move set to start from and one animation per attack; the wizard makes an own montage for each animation. Click **Create**.

![New Move Set wizard](Images/NewWizard.png)

3. The card of the new move set opens. The new move set appears in the hub's list and is selected. The text box below the buttons says "Created" and shows the full asset path. The new assets are not saved yet: click **Save All** in the Content Browser to keep them.

![Hub after the wizard](Images/HubCreated.png)

## The move set card

The card shows everything about one move set. Open it from the hub (**Open**) or double-click the asset in the Content Browser.

![Move Set card](Images/Card.png)

- **Properties**: the data of the move set.

![Properties section](Images/CardProperties.png)

- **Preview**: a picture of the first part that is set.

![Preview section](Images/CardPreview.png)

- **Parts**: every asset the move set uses. **own** parts lie in the move set's folder and are renamed, duplicated and deleted with it; **shared** parts are used by other assets too and are never touched. **Open** opens a part in its editor, **Create** makes a new own part.

![Parts section, Replace highlighted](Images/CardParts.png)

- To use another asset for a part, click **Replace** and click the asset in the list.

![Replace list open](Images/CardReplacePart.png)

- **Summary**: what the data means in practice.

![Summary section](Images/CardSummary.png)

- **Validation**: problems of the data, refreshed on every change. **Validate** checks again.

![Validation without problems](Images/CardValidation.png)

When a value does not work, Validation shows the problem in red right away, for example a move set without attacks.

![Validation with an error](Images/CardValidationError.png)

## Install the AI agent skills

The skills let an AI coding agent extend the plugin with you (see *AI agent skills* in the README).

1. On the main menu, click **Tools**, point to **CoopKit**, then click **Install AI Agent Skills**. The CoopKit Hub has the same button.

![Tools, CoopKit, Install AI Agent Skills](Images/ToolsMenuCoopKit.png)

![Install AI Agent Skills button in the hub](Images/HubSkillsButton.png)

2. A message lists the skill folders that were copied into your project. Click **OK**. If some folders could not be copied, the message shows a warning icon and lists them.

![Skills installed](Images/SkillsInstalled.png)

3. If the skills are already in the project, the plugin asks first. **Yes** replaces them and removes any change you made to their files; **No** keeps them.

![Replace the installed skills?](Images/SkillsReplace.png)

## Play the demo with the mouse

1. Show the plugin's content (see above), double-click `/CoopKitCombat/Maps/L_CombatDemo`, then click **Play** in the toolbar.

![Play button in the toolbar](Images/PlayButton.png)

2. The panel in the top left corner shows your health, stamina and action, and the health of the four training dummies. To fight next to a second player, click **Play with 2 windows**: Play restarts with two windows. Your Play settings stay as they were.

![One window with Play with 2 windows](Images/DemoOneWindow.png)

3. Click **Go to the Idle dummy**, then **Attack**. The dummy loses health, in every window.

![The Idle dummy is hit](Images/DemoHit.png)

4. Click **Go to the Block dummy**, then **Attack**. The dummy blocks the hit and keeps its health.

![The Block dummy blocks](Images/DemoBlocked.png)

5. Click **Raise block** to block, **Lower block** to stop, and **Dodge back** to dodge.

![You block](Images/DemoBlocking.png)
