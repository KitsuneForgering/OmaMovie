# Testes da libs/base (binário único com uma suíte Cest por arquivo).
$(eval $(call oma_test,base_tests,$(wildcard tests/base/*.cpp),base))
