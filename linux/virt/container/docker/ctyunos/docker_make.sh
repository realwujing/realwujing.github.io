#!/usr/bin/env bash
#
# Create a base CTyunOS Docker image.
#
# This script is useful on systems with yum installed
#Exit immediately if a simple command exits with a non-zero status.
set -e

#basename $0 : show script file name
usage() {
        cat << EOOPTS
$(basename $0) [OPTIONS] [name]
OPTIONS:
  -p "<packages>"  The list of packages to install in the container.
                   The default is blank. Can use multiple times.
  -g "<groups>"    The groups of packages to install in the container.
                   The default is "Core". Can use multiple times.
  -y <yumconf>     The path to the yum config to install packages from.
                   The default is /etc/yum.repos.d/ctyunos.repo
  -t <tag>         Specify Tag information.
                   default is reffered at /etc/ctyunos-release + date string
EOOPTS
        exit 1
}

# option defaults
yum_config=/etc/yum.repos.d/ctyunos.repo
# for names with spaces, use double quotes (") as install_groups=('Core' '"Compute Node"')
install_groups=()
install_packages=()
file="$target"/etc/ctyunos-release
if [ -r "$file" ]; then
        version="$(sed 's/^[^0-9\]*\([0-9.]\+\).*$/\1/' "$file")"
fi
version+="-"`date +%Y%m%d%H%M%S`

while getopts ":y:p:g:t:h" opt; do
        case $opt in
                y)
                        yum_config=$OPTARG
                        ;;
                h)
                        usage
                        ;;
                p)
                        install_packages+=("$OPTARG")
                        ;;
                g)
                        install_groups+=("$OPTARG")
                        ;;
                t)
                        version="$OPTARG"
                        ;;
                \?)
                        echo "Invalid option: -$OPTARG"
                        usage
                        ;;
        esac
done
shift $((OPTIND - 1))
name=$1

if [[ -z $name ]]; then
        name="ctyunos"
fi

# default to Core group if not specified otherwise
if [ ${#install_groups[*]} -eq 0 ]; then
        install_groups=('Core')
fi

target=$(mktemp -d --tmpdir $(basename $0).XXXXXX)

set -x

mkdir -m 755 "$target"/dev
mknod -m 600 "$target"/dev/console c 5 1
mknod -m 600 "$target"/dev/initctl p
mknod -m 666 "$target"/dev/full c 1 7
mknod -m 666 "$target"/dev/null c 1 3
mknod -m 666 "$target"/dev/ptmx c 5 2
mknod -m 666 "$target"/dev/random c 1 8
mknod -m 666 "$target"/dev/tty c 5 0
mknod -m 666 "$target"/dev/tty0 c 4 0
mknod -m 666 "$target"/dev/urandom c 1 9
mknod -m 666 "$target"/dev/zero c 1 5

# amazon linux yum will fail without vars set
if [ -d /etc/yum/vars ]; then
        mkdir -p -m 755 "$target"/etc/yum
        cp -a /etc/yum/vars "$target"/etc/yum/
fi

# install_groups is not ready for ctyunos yet. Disable it for now
#if [[ -n "$install_groups" ]]; then
#        yum -c "$yum_config" --setopt=install_weak_deps=False --installroot="$target" --releasever=/ --setopt=tsflags=nodocs \
#                --setopt=group_package_types=mandatory -y groupinstall "${install_groups[@]}"
#fi

if [[ -n "$install_packages" ]]; then
        yum -c "$yum_config" --setopt=install_weak_deps=False --installroot="$target" --releasever=/ --setopt=tsflags=nodocs \
                --setopt=group_package_types=mandatory -y install "${install_packages[@]}"
fi

yum -c "$yum_config" --setopt=install_weak_deps=False --installroot="$target" -y clean all

mkdir -p "$target"/etc/sysconfig/
cat > "$target"/etc/sysconfig/network << EOF
NETWORKING=yes
HOSTNAME=localhost.localdomain
EOF

# effectively: febootstrap-minimize --keep-zoneinfo --keep-rpmdb --keep-services "$target".
#  locales
#rm -rf "$target"/usr/{{lib,share}/locale,{lib,lib64}/gconv,bin/localedef,sbin/build-locale-archive}
#  docs and man pages
rm -rf "$target"/usr/share/{man,doc,info,gnome/help}
#  cracklib
rm -rf "$target"/usr/share/cracklib
#  i18n
rm -rf "$target"/usr/share/i18n
#  yum cache
rm -rf "$target"/var/cache/yum
mkdir -p --mode=0755 "$target"/var/cache/yum
#  sln
rm -rf "$target"/sbin/sln
#  ldconfig
rm -rf "$target"/etc/ld.so.cache "$target"/var/cache/ldconfig
mkdir -p --mode=0755 "$target"/var/cache/ldconfig

tar --numeric-owner -c -C "$target" . | docker import - $name:$version

#Not every image has bash, so abondon test
#docker run -i -t --rm $name:$version /bin/bash -c 'echo success'
docker save $name:$version | xz - > $name-$version.tar.xz
rm -rf "$target"