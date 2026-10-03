# ADR-0001 — Build com GNU Make, C++23, testes com Cest, convenções de código

- **Estado:** Aceito (2026-10-02)
- **Marco:** M0

## Contexto

O projeto precisa de build reproduzível, sanitizers, testes e lint desde o primeiro
commit (`Docs/implementation-plan.md`, M0). O plano original propunha CMake; o mantenedor
prefere não usar CMake. O framework de testes estava em aberto (GoogleTest ou Catch2); o
mantenedor escolheu o Cest, framework header-only de autoria própria.

## Decisão

### Build: GNU Make, não recursivo
- Um único `Makefile` na raiz. Cada lib declara seus fontes em `libs/<nome>/module.mk`
  com `oma_library`; cada suíte de testes em `tests/<nome>/module.mk` com `oma_test`.
- Modos por `BUILD=debug|release|asan|tsan`, cada um em `build/<modo>/`.
- **O grafo de dependências do CLAUDE.md §5.2 é verificado pelo Make**
  (`ALLOWED_DEPS_<lib>`): dependência proibida ou lib não registrada é erro antes de compilar.
- Dependências de headers automáticas (`-MMD -MP`); objetos dependem do Makefile.
- `make compdb` gera `compile_commands.json` para clangd e clang-tidy.
- Libs do projeto são estáticas (`liboma_<nome>.a`).

### Linguagem
- **C++23** (`-std=c++23`, sem extensões GNU), para `std::expected`, `std::move_only_function`,
  `std::format`, `std::source_location`. GCC 16 e Clang 22 (Arch) suportam.
- `__int128` é usado em `libs/base` para aritmética temporal exata, via `__extension__`.

### Testes: Cest
- `third_party/cest/cest.h`, vendorizado e fixado por commit (ver `third_party/cest/README.md`).
- Testes incluem `tests/support/oma_test.hpp`, nunca `cest.h` direto. Esse header configura
  o Cest, contorna um warning de pragma do `cest.h` em C++ e acrescenta overloads para
  inteiros sem sinal.
- Um binário por lib (`build/<modo>/tests/<lib>_tests`), com uma suíte por arquivo e um
  `main.cpp` que chama as funções `run_*_tests()`.
- `make test FILTER=<padrão>` usa o filtro do Cest; `JUNIT_DIR=<dir>` grava JUnit (CI).

Regras impostas pelo Cest (macros que recebem blocos de código):
1. `oma_test.hpp` é o **último** include do arquivo de teste.
2. **Sem vírgulas no nível de topo** dentro de `describe`/`it`: argumentos de template com
   vírgula, capturas `[&a, &b]` e `{1, 2}` quebram o bloco. Use `auto`, `[&]`, funções
   auxiliares ou parênteses.
3. **APIs do projeto não usam os nomes** `describe`, `test`, `it`, `expect`, `bench`,
   `beforeEach`, `afterEach`, `beforeAll`, `afterAll` (viram macros nos testes). Por isso
   `oma::Error` tem `summary()`, não `describe()`.
4. `expect()` só na thread do teste.

### Warnings, formatação e lint
- Libs: `-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
  -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2
  -Wimplicit-fallthrough -Werror`.
- Testes: `-Wall -Wextra -Wshadow -Werror` (as macros do Cest usam casts no estilo C).
- `.clang-format` (base LLVM, 4 espaços, 100 colunas); `make format` / `make format-check`.
- `.clang-tidy` com `WarningsAsErrors: '*'`; `make tidy` roda nas libs.

### Convenções de nome
| Elemento | Convenção | Exemplo |
|---|---|---|
| Tipos, enums, aliases | `PascalCase` | `RationalTime`, `JobState` |
| Valores de enum | `PascalCase` | `Rounding::Nearest` |
| Funções, métodos, variáveis | `snake_case` | `time_to_frame` |
| Membros privados | `snake_case_` | `timebase_` |
| Constantes (`constexpr`, globais) | `kPascalCase` | `kCategoryCount` |
| Namespaces | `snake_case` | `oma::frame_rates` |
| Headers | `#pragma once`, `include/oma/<lib>/<nome>.hpp` | |

Identificadores, comentários de código e mensagens de log em inglês (CLAUDE.md §22).

### Licença
O repositório é MIT. O FFmpeg do Arch é compilado com `--enable-gpl` (e Qt é LGPL/GPL):
binários distribuídos que linkam esse FFmpeg ficam sujeitos à GPL. MIT é compatível com a
GPL, então o código continua MIT; a distribuição binária segue os termos da GPL. Revisar
quando o empacotamento (M7) for definido.

## Alternativas consideradas
- **CMake + Ninja**: padrão do Qt 6 e do ecossistema; rejeitado por preferência do mantenedor.
- **Meson**: bom suporte a Qt e PkgConfig; não proposto.
- **GoogleTest / Catch2**: mais recursos (fixtures parametrizadas, matchers ricos); o Cest
  foi escolhido por ser do mantenedor, header-only e suficiente para o M0.

## Consequências
- Qt (M4/M6) sem CMake exige chamar `moc`, `rcc` e `qmlcachegen`/`qmltyperegistrar`
  pelo Makefile, usando `pkg-config` para Qt6. `qt_add_qml_module` é exclusivo do CMake;
  o registro de tipos QML terá de ser feito à mão ou com regras próprias. Avaliar no M4.
- Limitações do Cest (vírgulas, nomes reservados) precisam ser seguidas por todos os
  testes; violações aparecem como erros de compilação confusos dentro das macros.
- Problemas encontrados no Cest para reportar ao upstream: `#pragma GCC diagnostic ignored
  "-Wstrict-prototypes"` gera `-Wpragmas` em C++; redefinição de `_DEFAULT_SOURCE`.
