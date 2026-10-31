# Deploying the scriptable fork to its own subdomain

The vanilla web port is served from `s3://play-endless-sky.com/live` by two
CloudFront distributions (E2TZUW922XPLEF and E3D0Y4DMGSVPWC). The scriptable
fork goes in the same bucket, under `live/scriptable/`, served by a new
distribution on its own subdomain (below, `code.play-endless-web.com`; any
name works). A separate domain also keeps its saves apart from vanilla's,
because browsers keep IndexedDB per origin.

Nothing below has been run yet. Each step needs credentials with admin access
to the AWS account, so run them from a machine that has them, checking each
result before the next.

## 1. Look at what's there

```sh
aws cloudfront get-distribution-config --id E3D0Y4DMGSVPWC > vanilla-a.json
aws cloudfront get-distribution-config --id E2TZUW922XPLEF > vanilla-b.json
jq '.DistributionConfig | {Aliases, Origins: .Origins.Items[] | {DomainName, OriginPath, OriginAccessControlId, S3OriginConfig}}' vanilla-*.json
aws route53 list-hosted-zones
aws s3api get-bucket-policy --bucket play-endless-sky.com
```

Pick the distribution whose alias is `play-endless-web.com` as the template,
and note whether the bucket policy names each distribution (an origin access
control policy with an `AWS:SourceArn` condition) or is public.

## 2. Certificate (must be in us-east-1 for CloudFront)

```sh
aws acm request-certificate --region us-east-1 \
  --domain-name code.play-endless-web.com --validation-method DNS
aws acm describe-certificate --region us-east-1 --certificate-arn ARN \
  --query 'Certificate.DomainValidationOptions[0].ResourceRecord'
```

Add that CNAME to the domain's DNS (Route 53: `change-resource-record-sets`
with an UPSERT), then wait for `aws acm wait certificate-validated`.

## 3. Distribution

From the template's `DistributionConfig`, make a new config:

- `CallerReference`: something new, e.g. `scriptable-<date>`
- `Aliases`: `{"Quantity": 1, "Items": ["code.play-endless-web.com"]}`
- the S3 origin's `OriginPath`: `/live/scriptable`
- `ViewerCertificate.ACMCertificateArn`: the new certificate
- `Comment`: "Endless Sky, scriptable fork"

```sh
aws cloudfront create-distribution --distribution-config file://scriptable.json
```

If the bucket policy names distributions, add the new one's ARN to it.

## 4. DNS

An alias record (Route 53) or CNAME for `code.play-endless-web.com` pointing at
the new distribution's `*.cloudfront.net` domain (alias hosted zone
`Z2FDTNDATAQYW2`).

## 5. Let CI deploy it

- The IAM user whose keys are in the endless-web repo's `aws-deploy`
  environment must be allowed `cloudfront:CreateInvalidation` on the new
  distribution, and `s3:PutObject`/`s3:ListBucket` under `live/scriptable/`
  (probably already true if its policy covers the whole bucket).
- In the endless-web repo, set the variable `SCRIPTABLE_DISTRIBUTION_ID` to
  the new distribution's ID (Settings > Environments > aws-deploy >
  Environment variables, or as a repository variable).
- If `aws-deploy` limits which branches may deploy, allow `scriptable`.

## 6. Deploy

Push the fork's `scriptable-ai` branch to the endless-web repo as
`scriptable`. The browser workflow builds it and, on that branch only, runs
`make deploy-scriptable`, which syncs `output/` to `live/scriptable/` and
invalidates the new distribution. Pushes to the vanilla `endless-web` branch
deploy as before; the vanilla deploy never deletes, so it leaves
`live/scriptable/` alone.

To deploy by hand instead, from a scriptable-ai checkout with emsdk active:

```sh
make deploy-scriptable SCRIPTABLE_DISTRIBUTION_ID=<id>
```
