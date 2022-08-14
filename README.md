# linux-learning

This is a collection of notes and resources for learning Linux.

## git submodules

- 自动初始化并更新仓库中的每一个子模块， 包括可能存在的嵌套子模块。

    1. 方式1

        ```bash
        git clone --recurse-submodules https://github.com/realwujing/linux-learning.git
        ```

    2. 方式2

        ```bash
        git clone https://github.com/realwujing/linux-learning.git
        git submodule update --init --recursive
        ```

- 递归地抓取子模块的更改并更新当前仓库中的每一个子模块， 包括可能存在的嵌套子模块。

    1. 方式1

        ```bash
        git pull --recurse-submodules
        ```

    2. 方式2

        ```bash
        git pull
        git submodule update --init --recursive
        ```

- 推送当前仓库中的每一个子模块， 包括可能存在的嵌套子模块。

    ```bash
    git push --recurse-submodules=on-demand
    ```

## More

- [7.11 Git 工具 - 子模块](https://git-scm.com/book/zh/v2/Git-%E5%B7%A5%E5%85%B7-%E5%AD%90%E6%A8%A1%E5%9D%97)
