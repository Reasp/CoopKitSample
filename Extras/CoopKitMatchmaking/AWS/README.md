# Amazon GameLift FlexMatch for CoopKit Matchmaking

These files set up matchmaking on [Amazon GameLift Servers FlexMatch](https://docs.aws.amazon.com/gamelift/latest/flexmatchguide/match-intro.html) in standalone mode for the **AWS FlexMatch (HTTP backend)** provider of CoopKit Matchmaking. FlexMatch groups the players of your queue into co-op matches of 2-4. Standalone mode only makes the match: your players then host and join it themselves (listen server or peer-to-peer), with no GameLift servers.

The files in this folder are under the MIT License (see `../LICENSE`). Copy them into your own repository and change them as you need.

Status: the template is checked offline. The backend code and `sync_flexmatch.mjs` are tested only with fake AWS clients and mocks of the AWS APIs. Neither the template nor the script has run against a live AWS account. Deploy to a test account first and read the Cost section before you do.

| File | What it is |
|---|---|
| `FlexMatchStack.json` | AWS CloudFormation template. One file: no build step, no S3 bucket. |
| `backend/api.js` | Source of the HTTP API (Lambda). Already inside the template. |
| `backend/events.js` | Source of the FlexMatch event handler (Lambda). Already inside the template. |
| `embed_backend.mjs` | Copies the two sources into the template after you change them (Node.js 18 or later, no packages). |
| `sync_flexmatch.mjs` | Sends a queue exported from Unreal to FlexMatch: a rule set, the configuration settings and the backend's `PLAYER_ATTRIBUTES`. Node.js 18 or later, no packages. |
| `rule_set.mjs` | The offline check of the exported rule set that `sync_flexmatch.mjs` runs first. |
| `aws_sign.mjs` | Signs the script's AWS requests (AWS Signature Version 4). |
| `read_export.mjs` | Reads the export file of Unreal. The same file is in the Nakama and PlayFab folders. |

AWS recommends that games send matchmaking requests through a backend service, not from the game client ([Add FlexMatch to a game client](https://docs.aws.amazon.com/gamelift/latest/flexmatchguide/match-client.html)). This template is a small backend of that kind. Your game never holds AWS keys.

## What the stack creates

```
Game (provider)  --HTTPS-->  Lambda function URL (backend/api.js)  --StartMatchmaking/StopMatchmaking-->  FlexMatch
      ^                               |  reads tickets                                                     |
      |                               v                                                                    | events
      +------- polls every 2 s -- DynamoDB table  <-- writes ticket status -- Lambda (backend/events.js) <-- SNS topic
```

- **FlexMatch rule set and matchmaking configuration** in standalone mode. One team of up to `MaxPlayers` players. After `FullMatchWaitSeconds` the team may start with `MinPlayers`. Only players with the same `Mode` attribute are matched.
- **SNS topic** that FlexMatch sends its events to. AWS calls event notifications the preferred way to track tickets ([Roadmap: standalone FlexMatch](https://docs.aws.amazon.com/gamelift/latest/flexmatchguide/match-tasks-safm.html)).
- **Two Lambda functions** (Node.js 24, arm64) with their log groups. Logs are kept for 7 days.
- **Lambda function URL**, the HTTPS address the game talks to.
- **DynamoDB table** with on-demand billing. Ticket records get an expiry time one hour after they are made (DynamoDB removes expired items some time later).
- **IAM roles**, one per function. They allow only the actions the function needs. The GameLift permissions (`StartMatchmaking`, `StopMatchmaking`) apply to all matchmaking configurations of the account, not only this one.

The stack has no servers that run all the time. You pay for requests, stored data and FlexMatch use (see Cost).

## Before you start

- An AWS account. You set it up and you pay its bill. CoopKit never sees your keys.
- The [AWS CLI version 2](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html), signed in with `aws configure` or `aws sso login`. The user needs permission to create CloudFormation stacks, IAM roles, Lambda functions, SNS topics, DynamoDB tables and GameLift matchmaking resources.
- Keep your AWS keys on your machine only. Never put them in the game, in Unreal config files or in your repository.
- Pick an AWS Region that offers FlexMatch, for example `us-east-1`. If stack creation fails because a resource type is not available in the Region, choose another Region.

## Deploy

In this folder, run:

```
aws cloudformation deploy --template-file FlexMatchStack.json --stack-name coopkit-flexmatch --capabilities CAPABILITY_IAM --region us-east-1
aws cloudformation describe-stacks --stack-name coopkit-flexmatch --region us-east-1 --query "Stacks[0].Outputs"
```

`CAPABILITY_IAM` allows the stack to create its two IAM roles. The second command prints the outputs. `ApiUrl` is the address for the game.

Parameters (add `--parameter-overrides Name=Value ...` to the deploy command):

| Parameter | Default | What it does |
|---|---|---|
| `MinPlayers` | 2 | Fewest players of a match (2-4). |
| `MaxPlayers` | 4 | Most players of a match (2-4). FlexMatch first looks for a full match. |
| `FullMatchWaitSeconds` | 10 | Seconds before a match with `MinPlayers` is allowed. Must be less than `RequestTimeoutSeconds`. |
| `RequestTimeoutSeconds` | 60 | Seconds a ticket may wait before FlexMatch times it out. |
| `DefaultMode` | coop | Mode of a player whose ticket has no `Mode` attribute: letters, digits and hyphens. |
| `TokenLifetimeSeconds` | 21600 | How long a player's session token is valid. |
| `ApiReservedConcurrency` | 0 | Most API requests handled at once, to cap cost if someone floods the API. 0 means no cap. New accounts with a low concurrency quota cannot reserve it, so keep 0 there. |

A rule set cannot be edited in AWS, so a change of the rule set makes a new one. The stack names the rule set after the stack and the four rule parameters, so CloudFormation can create the new rule set and switch the configuration to it. Keep the stack name to 80 characters or less: the rule set name has a limit of 128.

## Connect the game

1. Open your queue (*Matchmaking Queue Data*) and set **Provider** to **AWS FlexMatch (HTTP backend)**.
2. Set **Base Url** to `ApiUrl` **without the trailing slash**, for example `https://abcdefghijklmnop.lambda-url.us-east-1.on.aws`.
3. Set the queue's **Default Attributes** to `Mode = coop` (or pass `Mode` to *Find Match*). Players with different modes are never matched.
4. Optional settings: **Poll Interval Seconds** (2 by default), **Numeric Keys** for number attributes that you add to the rule set.

The rule set on AWS decides the match size and which attributes must match. The stack's own rule set comes from the stack parameters. To use the queue's *Min Players*, *Max Players*, *Relax After Seconds* and *Match Keys* instead, export the queue and run `sync_flexmatch.mjs` (see *Send a queue from Unreal*). Set *Search Timeout Seconds* of the queue a little above `RequestTimeoutSeconds`. The export already sets `RequestTimeoutSeconds` 5 seconds below the queue's search timeout (at least 1).

Check the backend without the game:

```
curl -X POST https://YOUR-API-URL/v1/sessions
curl -X POST https://YOUR-API-URL/v1/tickets -H "Authorization: Bearer TOKEN" -H "Content-Type: application/json" -d "{\"attributes\":{\"Mode\":\"coop\"}}"
curl https://YOUR-API-URL/v1/tickets/TICKET-ID -H "Authorization: Bearer TOKEN"
```

One ticket alone stays `SEARCHING` until it reaches `TIMED_OUT`, because a match needs at least `MinPlayers`.

## Send a queue from Unreal

The stack starts with its own rule set: one `Mode` attribute and the sizes from the stack parameters. You can replace it with the queue you set in Unreal.

1. Open your queue (*Matchmaking Queue Data*) with the **AWS FlexMatch (HTTP backend)** provider. Set *Min Players*, *Max Players*, *Relax After Seconds*, *Search Timeout Seconds*, *Match Keys*, *Default Attributes* and the provider's *Numeric Keys*.
2. Click **Export** on the *Export* tab of the queue window. You can also run the commandlet `-run=CKMatchmakingExport`. Unreal writes `<Project>/CoopKit/Matchmaking/<QueueName>_FlexMatch.json` and sends nothing.
3. Find two names of your stack:

   ```
   aws cloudformation describe-stacks --stack-name coopkit-flexmatch --region us-east-1 --query "Stacks[0].Outputs"
   aws cloudformation describe-stack-resource --stack-name coopkit-flexmatch --logical-resource-id ApiFunction --region us-east-1 --query "StackResourceDetail.PhysicalResourceId" --output text
   ```

   The first command shows `MatchmakingConfigurationName`. The second prints the name of the API function.
4. In your project's root folder (not in the plugin folder, which is packaged), put the keys of an AWS user into a `.env.local` file and add it to `.gitignore`. Run the commands below from this folder, because the script reads `.env.local` only from the current folder:

   ```
   AWS_ACCESS_KEY_ID=<access key id>
   AWS_SECRET_ACCESS_KEY=<secret access key>
   AWS_REGION=us-east-1
   ```

   Environment variables with the same names work too, and they take precedence. With temporary keys, also set `AWS_SESSION_TOKEN`. The user needs these permissions: `gamelift:ValidateMatchmakingRuleSet`, `gamelift:DescribeMatchmakingRuleSets`, `gamelift:CreateMatchmakingRuleSet`, `gamelift:DescribeMatchmakingConfigurations`, `gamelift:UpdateMatchmakingConfiguration`, `lambda:GetFunctionConfiguration` and `lambda:UpdateFunctionConfiguration`.
5. Run the script:

   ```
   node <path to this folder>/sync_flexmatch.mjs CoopKit/Matchmaking/Coop_FlexMatch.json --check
   node <path to this folder>/sync_flexmatch.mjs CoopKit/Matchmaking/Coop_FlexMatch.json --configuration <MatchmakingConfigurationName> --function <API function name> --diff
   node <path to this folder>/sync_flexmatch.mjs CoopKit/Matchmaking/Coop_FlexMatch.json --configuration <MatchmakingConfigurationName> --function <API function name>
   ```

   - `--check` checks the file. It needs no keys and sends nothing. `--dry-run` also prints the requests without sending them.
   - `--diff` only reads. It shows whether the rule set is on AWS, which configuration fields change (`on AWS -> after applying`) and the change of `PLAYER_ATTRIBUTES`.
   - Without an option the script applies. It validates the rule set with AWS, creates the rule set if it is not there, points the configuration to it, and sets `PLAYER_ATTRIBUTES` of the API function. The function's other settings stay. `--region` overrides `AWS_REGION`.

How the export fills FlexMatch:

| In Unreal | In FlexMatch |
|---|---|
| *Max Players* | Team `coop` with that many players |
| *Min Players* and *Relax After Seconds* | After that many seconds the team may start with *Min Players* (an expansion). With *Min Players* equal to *Max Players* there is no expansion. |
| A *Match Key* | A player attribute and a rule `Same<Key>` (batch distance): players of one match have the same value. Keys in *Numeric Keys* are numbers. |
| A *Default Attributes* value | The attribute's default in the rule set |
| *Search Timeout Seconds* | `RequestTimeoutSeconds` of the configuration, a little lower |
| The match keys and their types | `PLAYER_ATTRIBUTES` of the API function, for example `Mode:S,Skill:N` |

Rule sets cannot be changed in AWS. The export names the rule set after the queue and a checksum of its body, for example `Coop-1a2b3c4d`. When you change the queue, the name changes, the script creates a new rule set and the configuration switches to it. Old rule sets stay until you delete them.

**After a sync the configuration uses a rule set that the stack did not create.** CloudFormation does not know about it, so a later stack update can undo the sync:

- An update that changes a rule set parameter (`MinPlayers`, `MaxPlayers`, `FullMatchWaitSeconds`, `DefaultMode`) or the configuration (`RequestTimeoutSeconds`) may point the configuration back to the stack's own rule set.
- An update that changes the API function (new backend code, `TokenLifetimeSeconds`, `ApiReservedConcurrency`) may set `PLAYER_ATTRIBUTES` back to `Mode:S`.

To be safe, run `sync_flexmatch.mjs` again after every stack update. `--diff` shows what the update changed back.

## HTTP API

| Call | Answer |
|---|---|
| `POST /v1/sessions` | `{"playerId", "token", "expiresIn"}`: a new guest player id and its token. |
| `POST /v1/tickets` with `{"attributes": {...}}` | `{"ticketId", "status": "QUEUED"}`. Only attributes declared in the rule set are sent to FlexMatch (`PLAYER_ATTRIBUTES` of the function, `Mode:S` by default). |
| `POST /v1/tickets` with `{"attributes": {...}, "members": ["p-..."]}` | A group: the caller and the members (at most 9, ids of 1-128 letters, digits and hyphens, none repeated) are the players of one FlexMatch ticket, all with the caller's attributes, so FlexMatch puts them in one match. The ticket belongs to the caller; the members have no ticket of their own. `400 InvalidGroup` for a bad member list. A backend deployed before this update does not know `members`: redeploy it with the command from the Deploy section. |
| `GET /v1/tickets/{id}` | `{"ticketId", "status", "statusReason", "statusMessage", "matchId", "players": [{"playerId", "team", "ticketId"}]}`. The status is FlexMatch's ticket status. |
| `DELETE /v1/tickets/{id}` | The same ticket view: `CANCELLED`, or `COMPLETED` when the match was made first. |

Every call except `/v1/sessions` needs `Authorization: Bearer <token>`. A player sees only their own tickets.

## Security

- The function URL is public: it has no AWS sign-in. It only accepts the calls above.
- Every `POST /v1/sessions` call creates a new guest id. Anyone who knows the URL can start tickets. Each ticket counts toward your FlexMatch use. To limit this:
  - set `ApiReservedConcurrency`;
  - replace `resolvePlayer` in `backend/api.js` with a check of your platform's sign-in token (for example an Epic Online Services ID token). It should return the platform's player id, so a player keeps one id.
- The token signing secret is made by the function on its first call and stored in the table. To revoke every token, delete the item `config#token-secret` from the table, then deploy the stack again with a changed function setting (for example `TokenLifetimeSeconds` plus 1), so that the API function starts fresh. A running function keeps the old secret in memory until it is replaced.
- The SNS topic is not encrypted. Encrypting it needs your own KMS key, which has a monthly cost ([SNS topic with server-side encryption](https://docs.aws.amazon.com/gamelift/latest/flexmatchguide/queue-notification-sns-sse.html)). The events hold player ids and ticket ids only.

## Change the backend

1. Edit `backend/api.js` or `backend/events.js`.
2. Run `node embed_backend.mjs` in this folder. It copies the code into `FlexMatchStack.json`.
3. Deploy again with the same command.

If you edit the rule set body in the template, also change the rule set `Name` (for example add `-v2`), or the stack update fails. To use more player attributes (for example a skill number), add them to the rule set in the template. Then add them to `PLAYER_ATTRIBUTES` of `ApiFunction` (for example `Mode:S,Skill:N`) and to **Numeric Keys** of the provider. The simpler way: set the keys in Unreal, export the queue and run `sync_flexmatch.mjs`, which sets both.

## Cost

Prices below are list prices from the AWS pricing pages, checked on 2026-11-14. They can change, and some differ by Region. The examples on the pages use their own Region and CPU type. Check the pages before you deploy. CloudWatch Logs (the function logs) has its own charges, which are not listed here.

| Service | Price | Free tier |
|---|---|---|
| FlexMatch ([pricing](https://aws.amazon.com/gamelift/pricing/flexmatch-pricing/)) | $20 per 1 million Player Packages, $1 per Matchmaking Hour | 50,000 Player Packages per month and 5 Matchmaking Hours. The page does not say whether the free tier is time limited. |
| Lambda ([pricing](https://aws.amazon.com/lambda/pricing/)) | $0.20 per 1 million requests, plus compute time ($0.0000166667 per GB-second in the x86 example; the stack uses arm64, which has its own rate on the page). The page lists no separate charge for function URLs. | 1 million requests and 400,000 GB-seconds per month |
| DynamoDB on-demand ([pricing](https://aws.amazon.com/dynamodb/pricing/on-demand/)) | $0.625 per million writes, $0.125 per million reads, $0.25 per GB of storage (examples on the page) | 25 GB of storage |
| SNS ([pricing](https://aws.amazon.com/sns/pricing/)) | Publishing is billed per request, and each 64 KB chunk counts as 1 request. AWS does not charge for per-message delivery to Lambda. | See the page |

New AWS accounts created since July 15, 2025 can use a Free plan: $100 in credits at once, up to $100 more to earn, for up to 6 months. AWS also lists 30+ services as always free within monthly limits on both the Free and Paid plans ([AWS Free Tier](https://aws.amazon.com/free/)). Check there which of the services above are included.

What drives the bill:

- **Player Packages.** AWS explains how it counts them on the pricing page; read that before you estimate. This stack sends one player per ticket, or the caller and all members of a group when the game sends `members`. A group ticket has several players, so check how AWS counts them before you estimate.
- **Matchmaking Hours.** These depend on your rule set and on how many players wait. AWS's own pricing examples assume 50 ms to 10 s of matchmaking per match.
- **Lambda and DynamoDB.** Every API call is one Lambda request. A status poll reads one table item. A waiting player polls every 2 seconds, so a one-minute search is about 30 requests.

Example of a test session, with assumptions: 100 tickets that each wait 1 minute, every match has 2 players (50 matches), every match takes 10 s of matchmaking.

- About 3,000 Lambda requests: $0.0006 (compute time not included).
- About 3,500 DynamoDB operations: below $0.003.
- 100 Player Packages: $0.002, if each ticket counts as one.
- Matchmaking Hours: 50 matches of 10 s are about 0.14 h, $0.14.

That is about $0.15 at list price under these assumptions. Your numbers will differ. The amounts of this example are far below the free-tier amounts in the table, if your account gets them. Watch your bill after the first test.

## Remove everything

```
aws cloudformation delete-stack --stack-name coopkit-flexmatch --region us-east-1
aws cloudformation wait stack-delete-complete --stack-name coopkit-flexmatch --region us-east-1
```

This deletes every resource the stack created, including the table (and its data), the log groups, the topic, the rule set and the configuration. Nothing in the template is set to stay after deletion. Afterwards, the game's requests to the old URL fail.

Rule sets that `sync_flexmatch.mjs` created are not part of the stack, so they stay. List them and delete them after the stack is gone (AWS does not delete a rule set that a configuration still uses):

```
aws gamelift describe-matchmaking-rule-sets --region us-east-1 --query "RuleSets[].RuleSetName"
aws gamelift delete-matchmaking-rule-set --name Coop-1a2b3c4d --region us-east-1
```
