# mount-net — systemd network share manager

Adds WebDAV (HTTP/HTTPS), FTP/FTPS, Samba (SMB) and SFTP shares as systemd
`.mount`/`.automount` units — no fstab, no manual config. Credentials are
stored root-only (`/etc/davfs2/secrets`, `/etc/cifs-credentials`); ftp embeds
its password in the unit file.

## Install

```bash
sudo apt install -y davfs2 cifs-utils sshfs curlftpfs
sudo curl -fsSL https://raw.githubusercontent.com/kunshakolime/debian-13-tricks/main/setups/mount-net/mount-net -o /usr/local/bin/mount-net && sudo chmod +x /usr/local/bin/mount-net
```

Installs the backends — `davfs2` (webdav), `cifs-utils` (smb), `sshfs` (sftp),
`curlftpfs` (ftp) — plus the `mount-net` script to `/usr/local/bin`.

## Usage

```bash
mount-net add                           # fully interactive — picks type (default
                                        # webdav), name, source, credentials,
                                        # mountpoint and mode
mount-net add sftp user@host:/pub nas1 /mnt/nas1 --user u --pass p
mount-net status                        # shares + mode + mounted state
mount-net keep nas1                     # mount at boot, never auto-unmount
mount-net auto nas1                     # mount on access, idle-unmount (default)
mount-net mount nas1 / mount-net umount nas1   # now
mount-net edit nas1 / mount-net remove nas1
```

If a mount can't be reached right now, the share is still saved with a warning
(it retries on access in auto mode, at boot in keep mode).
