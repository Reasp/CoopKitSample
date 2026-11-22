# CoopKit Matchmaking: deployment extras

Free deployment files for the matchmaking services that CoopKit Matchmaking talks to. They are licensed under the MIT
License (see `LICENSE` in this folder), unlike the plugin itself, so you may copy them into your own infrastructure repository.

The same files are published in the free CoopKitSample project on GitHub.

Each service has its own folder. Its README says what it creates, what it costs as of the date in the README, and how to remove what it created.

| Folder | Service | Sync script |
|---|---|---|
| `Nakama` | A local Heroic Labs Nakama server in Docker (Nakama and PostgreSQL) for the Nakama provider, with a small server module that holds your queues. | `sync_queue.mjs` |
| `AWS` | An AWS CloudFormation stack for Amazon GameLift FlexMatch in standalone mode, with a small backend (Lambda, SNS, DynamoDB), for the AWS FlexMatch (HTTP backend) provider. | `sync_flexmatch.mjs` |
| `PlayFab` | A co-op matchmaking queue for Azure PlayFab and a Node.js script that sends it to your PlayFab title, for the Azure PlayFab provider. | `set_queue.mjs` |

## From a queue in Unreal to the service

You set the queue once, in Unreal. Then a script copies it to the service.

1. Open the queue (*Matchmaking Queue Data*) and set its fields and its **Provider**.
2. Export it. Use the **Export** button on the *Export* tab of the queue window, or the commandlet `-run=CKMatchmakingExport`. Unreal writes one file, `<Project>/CoopKit/Matchmaking/<QueueName>_<Platform>.json`. Unreal sends nothing over the network.
3. Run the sync script of the service with that file, in this order:
   - `--check` checks the file. It needs no keys and sends nothing.
   - `--diff` shows what would change on the service. It only reads.
   - No option: the script applies the change.

The file lists the fields that the service cannot take, for example *Relax After Seconds* on PlayFab and Nakama. The scripts print them as notes.

The scripts need Node.js 18 or later and no packages. Keys stay on your machine, in environment variables or in a `.env.local` file in the folder you run the script from. Never put keys into the game, into Unreal config files or into your repository (add `.env.local` to `.gitignore`). Each folder's README says which keys its script needs.

The scripts were tested only against mocks of the services. They have not run against live AWS, PlayFab or Nakama. Try them on a test account or a local server first.
