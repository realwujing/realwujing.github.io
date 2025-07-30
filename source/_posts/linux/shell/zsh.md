---
date: 2024/06/04 11:46:48
updated: 2024/06/04 11:46:48
---

# zsh

## oh-my-zsh

Install oh-my-zsh now:

    ```bash
    sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"
    ```

- [Install oh-my-zsh now](https://ohmyz.sh/#install)

### zsh-completions

oh-my-zsh

Clone the repository inside your oh-my-zsh repo:

    ```bash
    git clone https://github.com/zsh-users/zsh-completions ${ZSH_CUSTOM:-${ZSH:-~/.oh-my-zsh}/custom}/plugins/zsh-completions
    ```

Add it to FPATH in your .zshrc by adding the following line before source "$ZSH/oh-my-zsh.sh":

    ```bash
    fpath+=${ZSH_CUSTOM:-${ZSH:-~/.oh-my-zsh}/custom}/plugins/zsh-completions/src
    ```

- [https://github.com/zsh-users/zsh-completions](https://github.com/zsh-users/zsh-completions)

### zsh-autosuggestions

Oh My Zsh

Clone this repository into $ZSH_CUSTOM/plugins (by default ~/.oh-my-zsh/custom/plugins)

    ```bash
    git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions
    ```

Add the plugin to the list of plugins for Oh My Zsh to load (inside ~/.zshrc):

    ```bash
    plugins=( 
        # other plugins...
        zsh-autosuggestions
    )
    ```

Start a new terminal session.

- [https://github.com/zsh-users/zsh-autosuggestions/blob/master/INSTALL.md](https://github.com/zsh-users/zsh-autosuggestions/blob/master/INSTALL.md)

### zsh-syntax-highlighting

Oh-my-zsh

Clone this repository in oh-my-zsh's plugins directory:

    ```bash
    git clone https://github.com/zsh-users/zsh-syntax-highlighting.git ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-syntax-highlighting
    ```

Activate the plugin in ~/.zshrc:

    ```bash
    plugins=( [plugins...] zsh-syntax-highlighting)
    ```

Restart zsh (such as by opening a new instance of your terminal emulator).

- [https://github.com/zsh-users/zsh-syntax-highlighting/blob/master/INSTALL.md](https://github.com/zsh-users/zsh-syntax-highlighting/blob/master/INSTALL.md)
