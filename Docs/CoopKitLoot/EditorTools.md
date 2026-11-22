# CoopKitLoot: editor tools

Step-by-step pictures of the editor tools of CoopKitLoot. You do everything with the mouse; the only typing is a name for a new asset. Your editor may look slightly different from the pictures.

The pictures are in `Docs/Images`.

## Show the plugin's content

The plugin's own assets (examples, demo maps) live in the plugin folder. To see them in the Content Browser, click **Settings** in the Content Browser and turn on **Show Plugin Content**.

![Content Browser Settings menu with Show Plugin Content](Images/ContentBrowserSettings.png)

## Create a loot table

1. In the Content Browser, open the folder for the new asset.
2. Click **Add**, point to **CoopKit**, then click **Loot Table Data**.
3. Enter a name for the asset. The New Loot Table wizard opens (see below).

![Add menu, CoopKit submenu, Loot Table Data entry](Images/CreateAsset.png)

## Open the CoopKit Hub

The hub shows the entities of every CoopKit plugin in your project in one window. CoopKitLoot adds the category **Loot Table**.

1. On the main menu, click **Tools**, then click **CoopKit Hub**.

![Tools menu with the CoopKit Hub entry](Images/ToolsMenu.png)

2. The hub opens on the categories. Each entity type has a button that shows the number of its entities and the name of the plugin that adds it.

![CoopKit Hub with the categories, Loot Table highlighted](Images/Hub.png)

## Find a loot table

1. Click the category **Loot Table**. The left side now lists only this category. It also has a search box, the name box and **New Loot Table**. Click **< Back** to return to the categories.

![Loot Table category open, Back highlighted](Images/HubPickType.png)

2. Click a loot table in the list. The list shows the table's display name and its asset name. The right side shows its name, type, asset path and id. **Open** opens its card in a separate window (a double click in the list does the same).

![Loot Table entities, Need or Greed selected](Images/HubEntities.png)

3. **Where Used** shows the assets that use the selected table, for example the demo's player controller.

![Where Used result](Images/HubWhereUsed.png)

4. **Check Project** checks every entity of every type: data errors and duplicate ids. The result appears in the text box below the buttons.

![Check Project result](Images/HubCheckProject.png)

## Make a new loot table with the wizard

1. In the **Loot Table** category, click the name box and enter a name (letters, digits, underscores). The button **New Loot Table** turns on.

![Name entered, New Loot Table button highlighted](Images/HubNewName.png)

2. Click **New Loot Table**. The wizard opens. Type a **Display Name** if you like, pick the sharing **Mode**, and pick a **Drop Mesh** and an **Icon** if you have them. With **Create Rolls Curve** on, the table gets its own rolls curve in its folder. Click **Create**.

![New Loot Table wizard](Images/NewWizard.png)

3. The new table appears in the hub's list and is selected, and its card opens. The table is made in a folder of its own with a starter entry. The text box below the buttons says "Created" and shows the asset path. The new asset is not saved yet: click **Save All** to keep it.

![Hub after the wizard](Images/HubCreated.png)

## The loot table card

The card shows everything about one loot table. Open it from the hub (**Open**) or double-click the asset in the Content Browser.

![Loot Table card](Images/Card.png)

- **Properties**: the data of the table. Open the **Loot** group to see the sharing mode, the entries, the rolls and the radii.

![Properties section](Images/CardProperties.png)

- **Preview**: the id of the table and the pictures of its parts. When no part is set, it says so.

![Preview section](Images/CardPreview.png)

- **Parts**: the **Drop Mesh**, the **Icon** and the **Rolls Curve** of the table. An **own** part lies in the table's folder and is used by nothing else. A **shared** part is used by other assets too and is never moved or deleted. **Open** opens a part in its editor, **Replace** points the slot at another asset, and **Create** makes a new rolls curve of the table's own.

![Parts section, Create of the Rolls Curve highlighted](Images/CardParts.png)

- **Summary**: the sharing mode, the rolls for 1 to 4 players and the chance of each item per roll.

![Summary section](Images/CardSummary.png)

- **Validation**: problems of the data, refreshed on every change. **Validate** checks again.

![Validation without problems](Images/CardValidation.png)

When a value does not work, Validation shows the problem in red right away, for example **Rolls** set to -1.

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

1. Show the plugin's content (see above), double-click `/CoopKitLoot/Maps/L_LootDemo`, then click **Play** in the toolbar.

![Play button in the toolbar](Images/PlayButton.png)

2. The panel in the top left corner has a button for each of the four ways of sharing, and **Play with 2 windows**.

![One window: the panel](Images/DemoOneWindow.png)

3. Click **Open the chest: Personal loot**. The panel lists your own drops. Click a row: the server gives you the item, and the panel says *The server gave you ...*.

![Personal loot on the floor](Images/DemoPersonal.png)

4. Click **Open the chest: Need or Greed**. A roll appears with **Need**, **Greed** and **Pass** and the seconds left.

![A Need or Greed roll](Images/DemoRoll.png)

5. Click **Need**. You are the only player, so you win, and the row says the item is yours. Click it to take it.

![The roll won and the item taken](Images/DemoWon.png)

6. Click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were.

![Window 1 after the restart](Images/DemoTwoWindowsHost.png)

7. In window 2, click **Open the chest: Free for all**. Both windows list the same free items. Click a row in window 2: that player gets the item, and the row goes away in both windows. Window 2 took the Blue gem here; both windows still list the other two items.

![Window 1: the free items](Images/DemoFreeForAllHost.png)

![Window 2: the Blue gem grabbed, its row gone](Images/DemoFreeForAllClient.png)

8. Click **Open the chest: Personal loot** in window 1. Each window lists only its own personal drops: the other player's personal loot is not on your machine at all.

![Window 1: its own personal loot](Images/DemoPersonalHost.png)

![Window 2: its own personal loot](Images/DemoPersonalClient.png)
