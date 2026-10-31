# Deploying the scriptable fork to code.play-endless-web.com

How the vanilla web port is hosted (account 700134640850, all looked up
2026-10-31):

- Bucket `play-endless-sky.com`, publicly readable. Vanilla is in `live/`.
- CloudFront E3D0Y4DMGSVPWC serves play-endless-web.com from `/live` (and
  `/editor`); E2TZUW922XPLEF serves play-endless-sky.com from `/live`.
- DNS for both domains is in Route 53; play-endless-web.com is zone
  `Z0956029IY6ZW2VC4C1K`.
- The wildcard certificate for `*.play-endless-web.com` (`1e94e90d-…`) expired
  in 2022, so code.play-endless-web.com has its own:
  `arn:aws:acm:us-east-1:700134640850:certificate/7ffb5cbf-203d-446a-af3e-82d9ad1eb68e`.
- CI (the endless-web repo's `aws-deploy` environment) deploys as the IAM
  user `endless-web-github-deployer`, whose policy `endless-sky-upload-live`
  allows only `live/` and the two distributions above.

The scriptable fork goes in the same bucket's `code/` folder, served by its
own distribution on code.play-endless-web.com. Being its own domain also
keeps its saves apart from vanilla's (browsers keep IndexedDB per origin).

Steps 1-3 were done on 2026-10-31: the distribution is `E2CB5CIFY6ZNI0`
(d3157hey3f3osz.cloudfront.net), and `SCRIPTABLE_DISTRIBUTION_ID` is set in
the `aws-deploy` environment. They're kept here as a record.

## 1. Distribution

A copy of play-endless-sky.com's distribution, serving `/code` under the new
name with the wildcard certificate:

```sh
CERT=arn:aws:acm:us-east-1:700134640850:certificate/7ffb5cbf-203d-446a-af3e-82d9ad1eb68e
aws cloudfront get-distribution-config --id E2TZUW922XPLEF --query DistributionConfig > base.json
jq --arg cert "$CERT" '
  .CallerReference = "scriptable-2026-10-31"
  | .Comment = "Endless Sky, scriptable fork"
  | .Aliases = {Quantity: 1, Items: ["code.play-endless-web.com"]}
  | .Origins.Items[0].OriginPath = "/code"
  | .ViewerCertificate.ACMCertificateArn = $cert
  | .ViewerCertificate.Certificate = $cert' base.json > code.json
aws cloudfront create-distribution --distribution-config file://code.json \
  --query 'Distribution.{Id:Id,Domain:DomainName}'
```

Note the new distribution's `Id` and `Domain` (`d….cloudfront.net`).

## 2. DNS

```sh
aws route53 change-resource-record-sets --hosted-zone-id Z0956029IY6ZW2VC4C1K \
  --change-batch '{"Changes": [{"Action": "CREATE", "ResourceRecordSet": {
    "Name": "code.play-endless-web.com", "Type": "A",
    "AliasTarget": {"HostedZoneId": "Z2FDTNDATAQYW2",
      "DNSName": "NEW_DOMAIN.cloudfront.net", "EvaluateTargetHealth": false}}}]}'
```

(`Z2FDTNDATAQYW2` is CloudFront's own zone ID, the same for every alias.)

## 3. Let CI deploy it

A separate policy for the existing deploy user, so the vanilla policy stays
as it is and no new keys are needed:

```sh
aws iam create-policy --policy-name endless-sky-upload-code --policy-document '{
  "Version": "2012-10-17",
  "Statement": [
    {"Effect": "Allow", "Action": "s3:ListBucket",
     "Resource": "arn:aws:s3:::play-endless-sky.com"},
    {"Effect": "Allow", "Action": ["s3:PutObject", "s3:GetObject"],
     "Resource": "arn:aws:s3:::play-endless-sky.com/code/*"},
    {"Effect": "Allow",
     "Action": ["cloudfront:CreateInvalidation", "cloudfront:GetInvalidation", "cloudfront:ListInvalidations"],
     "Resource": "arn:aws:cloudfront::700134640850:distribution/NEW_ID"}
  ]}'
aws iam attach-user-policy --user-name endless-web-github-deployer \
  --policy-arn arn:aws:iam::700134640850:policy/endless-sky-upload-code
```

In GitHub, give the endless-web repo the new distribution's ID, and check
whether `aws-deploy` limits which branches may use it:

```sh
gh variable set SCRIPTABLE_DISTRIBUTION_ID --repo thomasballinger/endless-web --env aws-deploy --body NEW_ID
gh api repos/thomasballinger/endless-web/environments/aws-deploy --jq .deployment_branch_policy
```

If that policy is not null, allow the `scriptable` branch in Settings >
Environments > aws-deploy.

## 4. Deploy

Push the fork's `scriptable-ai` branch to the endless-web repo as
`scriptable`:

```sh
git fetch tom-endless-sky scriptable-ai
git push origin tom-endless-sky/scriptable-ai:refs/heads/scriptable
```

The browser workflow builds it and, on that branch only, runs
`make deploy-scriptable`, which syncs `output/` to `code/` and invalidates
the new distribution. Pushes to the vanilla `endless-web` branch deploy as
before. To deploy by hand instead, from a scriptable-ai checkout with emsdk
active:

```sh
make deploy-scriptable SCRIPTABLE_DISTRIBUTION_ID=NEW_ID
```

To update the deployed game later, push `scriptable-ai` to `scriptable`
again.
