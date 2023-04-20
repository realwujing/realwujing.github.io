#! /bin/bash

sed -i 's/^|/-/g' $1

sed -i 's/ \{2,\}/ /g' $1

sed -i 's/ |$//g' $1

sed -i 's/^|/-/g' $1


sed -i 's/ //g' $1

sed -i 's/- $//g' $1

sed -i 's/--//g' $1

sed -i 's/- ://g' $1

sed -i '/\[/!s/^-/##/g' $1

sed -i '/^##/{G;}' $1