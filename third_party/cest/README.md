# Cest (vendored)

Framework de testes do OmaMovie (ADR-0001).

| Campo | Valor |
|---|---|
| Origem | https://github.com/KitsuneSemCalda/Cest |
| Commit | `3457ffcbc45deed4095ffe1151ef7d67dc135fc9` (v1.1.2 + 2 commits) |
| Arquivo | `cest.h` (sha256 `1c0d9a282b56429b679e8a1e8f9488ad97c4c805cffa7f362d010a6222ecdaba`) |
| Licença | BSD-3-Clause (`LICENSE`) |

Não edite `cest.h` aqui. Para atualizar: copie o `cest.h` de um commit/tag novo,
atualize esta tabela e rode todos os testes nos presets `debug`, `asan` e `tsan`.

Os testes não incluem este arquivo diretamente: usam `tests/support/oma_test.hpp`,
que configura o Cest e acrescenta overloads para os tipos do OmaMovie.
