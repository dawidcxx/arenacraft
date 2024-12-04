
export ZSH=$HOME/.oh-my-zsh

ZSH_THEME="devcontainers"

plugins=(git zsh-autosuggestions)

source $ZSH/oh-my-zsh.sh


DISABLE_AUTO_UPDATE=true
DISABLE_UPDATE_PROMPT=true

export PATH=$PATH:/usr/local/bin:/usr/local/tools
