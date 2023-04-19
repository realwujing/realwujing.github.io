#! /bin/bash

file=kernel.md

sed -i 's/^|/-/g' $file

sed -i 's/ \{2,\}/ /g' $file

sed -i 's/ |$//g' $file

sed -i 's/^|/-/g' $file


sed -i 's/ //g' $file

sed -i 's/- $//g' $file

sed -i 's/--//g' $file

sed -i 's/- ://g' $file

sed -i '/\[/!s/^-/##/g' $file

sed -i '/^##/{G;}' $file