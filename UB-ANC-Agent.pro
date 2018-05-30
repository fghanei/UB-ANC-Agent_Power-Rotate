
CONFIG += ordered

TEMPLATE = subdirs

# You can comment qgc_cli if you build it once and want to rebuild only agent.

SUBDIRS += \
    qgc_cli \
    agent \
