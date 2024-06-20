#!/bin/bash

toplevel=$STKHOME
name=meta
git submodule update --init --recursive
git checkout $(git config -f $toplevel/.gitmodules submodule.$name.branch || echo master)


git config --global alias.customUpdate '!toplevel=$STKHOME; name=meta; git submodule update --init --recursive; git checkout $(git config -f $toplevel/.gitmodules submodule.$name.branch)\'


git submodule foreach -q --recursive 'git checkout $(git config -f $toplevel/.gitmodules submodule.$name.branch || echo master)'


git config --global alias.checkoutSubmoduleBranch '!f() { branch_name=$(git config -f $toplevel/.gitmodules submodule.$name.branch || echo master); git checkout $branch_name; }; f'