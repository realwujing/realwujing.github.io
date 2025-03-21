---
date: 2024/12/26 21:02:44
updated: 2024/12/26 21:02:44
---

# debian10

debian10:

```bash
docker run -it \
  --name debian10 \
  -v /home/wujing/code/:/root/code \
  -v /home/wujing/code/rpmbuild:/root/rpmbuild \
  -v /home/wujing/Downloads:/root/Downloads \
  waffleimage/debian10:latest /bin/bash
```

debian11:

```bash
docker run -it \
  --name debian11 \
  -v /home/wujing/code/:/root/code \
  -v /home/wujing/code/rpmbuild:/root/rpmbuild \
  -v /home/wujing/Downloads:/root/Downloads \
  paritytech/debian11:latest /bin/bash
```

enable bash completion:

```bash
cat >> ~.bashrc << EOF
# enable bash completion
if [ -f /usr/share/bash-completion/bash_completion ]; then
        . /usr/share/bash-completion/bash_completion
fi
EOF
```
