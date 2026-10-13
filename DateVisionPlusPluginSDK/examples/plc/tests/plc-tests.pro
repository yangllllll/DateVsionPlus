TEMPLATE = subdirs
SUBDIRS = mock runner
mock.file = mock.pro
runner.file = runner.pro
runner.depends = mock
