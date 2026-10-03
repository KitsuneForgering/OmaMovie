# libs/timeline tests: model invariants, every edit with undo/redo, and per-instant evaluation.
# No GPU, media files or Qt.
$(eval $(call oma_test,timeline_tests,$(wildcard tests/timeline/*.cpp),timeline base))
