# CoopKitSave: editor tools

Step-by-step pictures of the editor tools of CoopKitSave. You do everything with the mouse; the only typing is a name for a new asset. Your editor may look slightly different from the pictures.

The pictures are in `Docs/Images`.

## Show the plugin's content

The plugin's own assets (examples, demo maps) live in the plugin folder. To see them in the Content Browser, click **Settings** in the Content Browser and turn on **Show Plugin Content**.

![Content Browser Settings menu with Show Plugin Content](Images/ContentBrowserSettings.png)

## Create a save profile

1. In the Content Browser, open the folder for the new asset.
2. Click **Add**, point to **CoopKit**, then click **Save Profile**.
3. Enter a name for the asset. The New Save Profile wizard opens (see below).

![Add menu, CoopKit submenu, Save Profile entry](Images/CreateAsset.png)

## Open the CoopKit Hub

The hub shows the entities of every CoopKit plugin in your project in one window. CoopKitSave adds the category **Save Profile**.

1. On the main menu, click **Tools**, then click **CoopKit Hub**.

![Tools menu with the CoopKit Hub entry](Images/ToolsMenu.png)

2. The hub opens on the categories. Each entity type has a button that shows the number of its entities and the name of the plugin that adds it.

![CoopKit Hub with the categories, Save Profile highlighted](Images/Hub.png)

## Find a save profile

1. Click the category **Save Profile**. The left side now lists only this category. It also has a search box, the name box and **New Save Profile**. Click **< Back** to return to the categories.

![Save Profile category open, Back highlighted](Images/HubPickType.png)

2. Click a save profile in the list. A save profile has no separate display name, so the list shows the asset's name. The right side shows its name, type and asset path. **Open** opens its card in a separate window (a double click in the list does the same).

![Save Profile entities, one selected](Images/HubEntities.png)

3. **Where Used** shows the assets that use the selected profile, for example the demo's game mode.

![Where Used result](Images/HubWhereUsed.png)

4. **Check Project** checks every entity of every type: data errors and duplicate ids. The result appears in the text box below the buttons.

![Check Project result](Images/HubCheckProject.png)

## Make a new save profile with the wizard

1. In the **Save Profile** category, click the name box and enter a name (letters, digits, underscores). The button **New Save Profile** turns on.

![Name entered, New Save Profile button highlighted](Images/HubNewName.png)

2. Click **New Save Profile**. The wizard opens. Pick a profile to start from, if you like, and type a **Slot Prefix** for your game's saves. Click **Create**.

![New Save Profile wizard](Images/NewWizard.png)

3. The new profile appears in the hub's list and is selected, and its card opens. The text box below the buttons says "Created" and shows the asset path. The new asset is not saved yet: click **Save All** to keep it.

![Hub after the wizard](Images/HubCreated.png)

## The save profile card

The card shows everything about one save profile. Open it from the hub (**Open**) or double-click the asset in the Content Browser.

![Save Profile card](Images/Card.png)

- **Properties**: the data of the profile.

![Properties section](Images/CardProperties.png)

- **Preview**: the name of the profile. A save profile has no picture to show.

![Preview section](Images/CardPreview.png)

- **Parts**: the other assets the profile uses. A save profile uses none, so the list is empty.

![Parts section](Images/CardParts.png)

- **Summary**: what is saved, when, and how players are matched.

![Summary section](Images/CardSummary.png)

- **Validation**: problems of the data, refreshed on every change. **Validate** checks again.

![Validation without problems](Images/CardValidation.png)

When a value does not work, Validation shows the problem in red right away, for example **Max Slots** set to -1.

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

1. Show the plugin's content (see above), double-click `/CoopKitSave/Maps/L_SaveDemo`, then click **Play** in the toolbar.

![Play button in the toolbar](Images/PlayButton.png)

2. The panel in the top left corner of the window saves and loads the game. To see a second player come back too, click **Play with 2 windows**: Play restarts with two windows. Your Play settings stay as they were.

![One window with Play with 2 windows](Images/DemoOneWindow.png)

3. In window 1, click **Save the game**. A **Load Quick save** button appears.

![Saved](Images/DemoSaved.png)

4. Click **Move every player and crate**. The players and the crates move to new spots.

![Everything moved](Images/DemoMoved.png)

5. Click **Load Quick save**. Every player and every crate is back where it was at the save, in window 2 too. The line *You stand at* shows the saved position in each window.

![Loaded: the host window](Images/DemoLoaded.png)

![Loaded: the second window](Images/DemoLoadedClient.png)
