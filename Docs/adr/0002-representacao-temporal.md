# ADR-0002 — Representação temporal

- **Estado:** Aceito (2026-10-02)
- **Marco:** M0 (tipos base); M5 (mapeamento de tempo dos clips)

## Contexto

Timestamps em `double` acumulam erro, não representam 29.97 fps exatamente e quebram
comparações de igualdade. Mídia real é frequentemente VFR (celulares, gravações de tela) e
mistura timebases (90 kHz no MPEG-TS, 1/30000 em MP4, amostras de áudio a 44.1/48 kHz).
A pesquisa de produto (`Docs/Research/capcut.md` §4.2) mostrou que speed ramps são um
requisito de criadores, o que exige mapear tempo da timeline para tempo da mídia por uma
curva, não só por velocidade constante.

## Decisão

### Tipos (`libs/base`, implementados no M0)
| Tipo | Definição | Invariantes |
|---|---|---|
| `Rational` | `num/den` em `int64` | Sempre normalizado: `den > 0`, `gcd == 1`; `INT64_MIN` rejeitado nos dois termos |
| `RationalTime` | `value * timebase` segundos | `timebase > 0` |
| `TimeRange` | `[start, start + duration)` | Mesmo timebase em início e duração; `duration >= 0`; fim calculado na criação |
| `FrameRate` | fps como `Rational` (30000/1001) | `> 0`; constantes em `oma::frame_rates` |
| `SampleRate` | Hz em `int32` | `> 0`; áudio endereçado em amostras |

Regras:
1. **Conversão sempre explícita**, com `Rounding::{Floor, Ceil, Nearest}` (Nearest desempata
   para longe do zero, como `AV_ROUND_NEAR_INF` do FFmpeg).
2. `rescale()` usa 128 bits; resultado fora de `int64` retorna `ErrorCode::Overflow`, nunca trunca.
3. **Comparação exata entre timebases** sem produtos cruzados maiores que 128 bits
   (comparação de frações por expansão em fração contínua, terminação como Euclides):
   `3003 @ 1/90000 == 1001 @ 1/30000`.
4. Aritmética (`plus`, `minus`) só entre tempos do mesmo timebase; misturar é erro.
5. Construtores validam e retornam `Result<T>`. Constantes conhecidas usam
   `Rational::literal()` / `FrameRate::literal()` (`consteval`): literal inválido não compila.
6. `double` só para exibição (`seconds_approx()`, `to_double_approx()`).
7. `AV_NOPTS_VALUE` vira `std::optional` vazio na borda de `libs/media` (M2).
8. `FrameRate` descreve taxa nominal; frames de mídia são localizados por PTS, nunca por
   `índice * duração` (VFR é caso normal).

### Mapeamento de tempo dos clips (desenho para o M5)
- Cada clip tem um `TimeMap`: função **monotônica, contínua e por partes** de tempo da
  timeline (local ao clip) para tempo da mídia.
- Segmentos iniciais: **linear** (velocidade constante, inclusive 1x) e **congelado**
  (freeze frame). Speed ramps entram como segmentos com velocidade interpolada entre
  pontos de controle (curva), avaliados em aritmética exata nos pontos de controle e com
  arredondamento explícito ao converter para PTS da mídia.
- Reverso é um segmento decrescente; o requisito de monotonicidade vale por segmento.
- A duração do clip na timeline é derivada do `TimeMap`, não armazenada separadamente.
- A v0.1 só cria mapas de um segmento linear, mas o modelo e o formato de projeto já
  usam `TimeMap`, para que speed ramps (v0.2) não exijam migração estrutural.

## Alternativas consideradas
- **`double` em segundos**: simples, mas impreciso e sem igualdade confiável.
- **Inteiro em timebase global fixa (ex.: 1/705600000, o "flick")**: representa as taxas
  comuns sem erro, mas não representa timebases arbitrárias de arquivos e ainda exige
  conversão na borda. Pode ser reconsiderado como timebase interna da timeline.
- **`std::chrono::duration<int64, std::ratio<...>>`**: ratio fixo em tempo de compilação;
  timebases de mídia só são conhecidos em runtime.

## Consequências
- Todo código de tempo trata `Result` e escolhe arredondamento: mais verboso, mas os
  pontos de perda de precisão ficam visíveis no código.
- Testes cobrem 23.976/29.97/59.94 fps, 44.1/48/96 kHz, dez horas em 90 kHz, arredondamento
  em valores negativos, empates e overflow (`tests/base/test_rational.cpp`, `test_time.cpp`).
- O `TimeMap` com curvas ainda precisa de um ADR complementar no M5 para a interpolação
  (escolha da curva e da avaliação numérica).
