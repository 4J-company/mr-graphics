# Publishing the image to ghcr.io

The image is built locally (`make image-build`) and pushed to
`ghcr.io/4j-company/mr-graphics-dev`. `make image-download` pulls the `:latest` tag.

## Token

1. Open <https://github.com/settings/tokens>.
2. **Generate new token** → **Generate new token (classic)**.
3. Enable only **`write:packages`**. `read:packages` is selected automatically. The `repo` scope is not required to push the image.
4. Copy the `ghp_...` string. GitHub shows it once. Do not commit it or paste it into chat.

If the organization has SAML SSO enabled, open the token list, click **Configure SSO**, and authorize the token for `4J-company`. Without that, a push to `ghcr.io/4j-company/...` is rejected. Check <https://github.com/orgs/4J-company/sso>. A 404 means SSO is off and this step is unnecessary.

## docker login

The username is the GitHub login. The **Password** field takes the token, not the account password. Typed characters are not shown.

```bash
docker login ghcr.io -u <github-login>
```

## Upload

```bash
make image-upload
```

The command publishes two tags of the same image:

- `ghcr.io/4j-company/mr-graphics-dev:latest` — the tag `make image-download` pulls
- `ghcr.io/4j-company/mr-graphics-dev:<id>` — the first 12 characters of the image id; this tag does not move on the next upload

The second push sends only the tag. Layers are not uploaded again.

The local image `mr-graphics-dev:local` (`IMAGE` in the Makefile) must already exist before the push.

## After the first upload

The package is created private. To let anyone pull it without a token, make it public in the organization package settings and link it to the `4J-company/mr-graphics` repository.

The GitHub Packages quota (500 MB for a free organization) does not currently apply to the Container Registry: image storage and bandwidth are free. The registry limit is 10 GB per compressed layer and 10 minutes per layer upload. This image fits: about 21 GB on disk, about 3.5 GB in the registry, and the largest layer is about 2.5 GB.
