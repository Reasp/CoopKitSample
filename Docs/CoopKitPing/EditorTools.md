# CoopKitPing: editor tools

Step-by-step pictures of the editor tools of CoopKitPing. You do everything with the mouse; the only typing is a name for a new asset. Your editor may look slightly different from the pictures.

The pictures are in `Docs/Images`.

## Show the plugin's content

The plugin's own assets (examples, demo maps) live in the plugin folder. To see them in the Content Browser, click **Settings** in the Content Browser and turn on **Show Plugin Content**.

![Content Browser Settings menu with Show Plugin Content](Images/ContentBrowserSettings.png)

## Create ping rules

1. In the Content Browser, open the folder for the new asset.
2. Click **Add**, point to **CoopKit**, then click **Ping Rules**. **Ping Type** in the same menu creates a ping type alone.
3. Enter a name for the asset. The New Ping Rules wizard opens (see below).

![Add menu, CoopKit submenu, Ping Rules entry](Images/CreateAsset.png)

## Open the CoopKit Hub

The hub shows the entities of every CoopKit plugin in your project in one window. CoopKitPing adds the category **Ping Rules**.

1. On the main menu, click **Tools**, then click **CoopKit Hub**.

![Tools menu with the CoopKit Hub entry](Images/ToolsMenu.png)

2. The hub opens on the categories. Each entity type has a button that shows the number of its entities and the name of the plugin that adds it.

![CoopKit Hub with the categories, Ping Rules highlighted](Images/Hub.png)

## Find ping rules

1. Click the category **Ping Rules**. The left side now lists only this category. It also has a search box, the name box and **New Ping Rules**. Click **< Back** to return to the categories.

![Ping Rules category open, Back highlighted](Images/HubPickType.png)

2. Click ping rules in the list. Ping rules have no separate display name, so the list shows the asset's name. The right side shows their name, type and asset path. **Open** opens their card in a separate window (a double click in the list does the same).

![Ping Rules entities, one selected](Images/HubEntities.png)

3. **Where Used** shows the assets that use the selected rules, for example the demo's game state.

![Where Used result](Images/HubWhereUsed.png)

4. **Check Project** checks every entity of every type: data errors and duplicate ids. The result appears in the text box below the buttons.

![Check Project result](Images/HubCheckProject.png)

## Make new ping rules with the wizard

1. In the **Ping Rules** category, click the name box and enter a name (letters, digits, underscores). The button **New Ping Rules** turns on.

![Name entered, New Ping Rules button highlighted](Images/HubNewName.png)

2. Click **New Ping Rules**. The wizard opens. Pick rules to start from, if you like (empty: the project's default rules). Leave **Create Types** on to get ping types of the rules' own, copies of the types of the rules to start from. Click **Create**.

![New Ping Rules wizard](Images/NewWizard.png)

3. The new rules appear in the hub's list and are selected, and their card opens. The text box below the buttons says "Created" and shows the asset path. The new assets are not saved yet: click **Save All** to keep them.

![Hub after the wizard](Images/HubCreated.png)

## The ping rules card

The card shows everything about one set of ping rules. Open it from the hub (**Open**) or double-click the asset in the Content Browser.

![Ping Rules card](Images/Card.png)

- **Properties**: the data of the rules.

![Properties section](Images/CardProperties.png)

- **Preview**: the name of the rules and a picture of one of the ping types they use.

![Preview section](Images/CardPreview.png)

- **Parts**: the **Quick Ping** type and one row per sector of the wheel (**Wheel 1**, **Wheel 2**, ...), then an empty row while the wheel has room. An **own** part lies in the rules' folder and is used by nothing else. A **shared** part is used by other assets too and is never moved or deleted. **Open** opens the ping type in its editor. **Create** makes a new ping type in the rules' folder and puts it in the row.

![Parts section, Replace highlighted](Images/CardParts.png)

- To use another ping type, click **Replace** and click the asset in the list. Replace in the empty row adds a sector; **None** removes a sector or empties the quick ping.

![Replace list open](Images/CardReplacePart.png)

- **Summary**: the wheel sector by sector, the rate limit and who sees pings.

![Summary section](Images/CardSummary.png)

- **Validation**: problems of the data, refreshed on every change. **Validate** checks again.

![Validation without problems](Images/CardValidation.png)

When a value does not work, Validation shows the problem in red right away, for example **Rate Limit Window Seconds** set to 0.

![Validation with an error](Images/CardValidationError.png)

## Edit a ping type

Double-click the ping type in the Content Browser. The data editor opens with three sections. A ping type has no parts of its own, so it has no card.

![Data editor of Ping Type](Images/Editor.png)

- **Properties** (left): the data of the asset.

![Properties section](Images/EditorProperties.png)

- **Summary** (top right): what the values mean in practice.

![Summary section](Images/EditorSummary.png)

- **Validation** (bottom right): problems of the data, refreshed on every change. **Validate** checks again.

![Validation without problems](Images/EditorValidation.png)

When a value does not work, Validation shows the problem in red right away, for example **Lifetime Seconds** set to 0.

![Validation with an error](Images/EditorValidationError.png)

Ping rules open in their card, described above.

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

1. Show the plugin's content (see above), double-click `/CoopKitPing/Maps/L_PingDemo`, then click **Play** in the toolbar.

![Play button in the toolbar](Images/PlayButton.png)

2. The panel in the top left corner of the window lists the ping types and the objects of the map. A second player sees your pings, so click **Play with 2 windows**: Play restarts with two windows. Your Play settings stay as they were.

![One window with Play with 2 windows](Images/DemoOneWindow.png)

![Both windows show the panel](Images/DemoStart.png)

3. In window 1, click a ping type, then an object, for example **Danger**, then **Crate 1**. Window 2 shows the ping under *Pings you see*, for example *Danger on Crate 1, by Player 1, 12 m*.

![Window 2 sees the host's ping](Images/DemoHostPing.png)

4. In window 2, click **What you look at**. Window 1 shows that ping *by Player 2*. **Clear my pings** appears when you have pings of your own. Click it to remove them.

![Window 1 sees the client's ping](Images/DemoClientPing.png)
