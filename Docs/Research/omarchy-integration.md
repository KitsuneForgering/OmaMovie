# Integração com o Omarchy

> Pesquisa feita em 2026-10-02 **inspecionando a instalação local do Omarchy
> 4.0.4** (`/usr/share/omarchy`, `~/.config/omarchy`, `~/.local/state/omarchy`).
> Caminhos e formatos podem mudar entre versões do Omarchy. Toda integração deve
> ter fallback e ficar isolada num único módulo do app (`CLAUDE.md` §11).

---

## 1. Resumo

O Omarchy 4 oferece pontos de integração concretos e estáveis o bastante para o OmaMovie usar:

| Ponto | Mecanismo | Uso no OmaMovie |
|---|---|---|
| **Tema** | `~/.local/state/omarchy/current/theme/colors.toml` + `omarchy-theme-color` | Cores da interface, troca ao vivo |
| **Opacidade de janela** | Regras do Hyprland em Lua | **Janela 100% opaca** (preview com cor correta) |
| **Gravações de tela** | `gpu-screen-recorder` → `$XDG_VIDEOS_DIR` | Fonte "Gravações" na biblioteca |
| **Shell** (Quickshell/QML) | Plugins com `manifest.json`, IPC | Widget de progresso de export (opcional) |
| **Menu do Omarchy** | `~/.config/omarchy/extensions/omarchy-menu.jsonc` | Atalhos: novo projeto, editar última gravação |
| **Notificações** | `omarchy-notification-send` / D-Bus | "Export concluído" |
| **Hooks** | `~/.config/omarchy/hooks/<nome>.d/` | Reagir a troca de tema e fonte |
| **Drivers** | Instalador do Omarchy por fabricante | Saber que caminho de hardware esperar |

Achado de produto: **o Kdenlive vem instalado por padrão no Omarchy**
(`install/omarchy-base.packages`), junto com OBS, mpv, `gpu-screen-recorder` e
`ffmpegthumbnailer`. O OmaMovie compete por esse lugar de editor padrão.

---

## 2. Contexto técnico do Omarchy 4

- Arch Linux + Hyprland; **a configuração do Hyprland é em Lua** (`default/hypr/*.lua`).
- **`omarchy-shell`**: uma instância única de **Quickshell** (QML) que hospeda
  barra, menus, painéis, notificações e overlays como plugins. **É a mesma tecnologia de UI do OmaMovie (Qt Quick/QML).**
- Mais de 440 comandos `omarchy-*` em `/usr/share/omarchy/bin`, com metadados (`omarchy:summary`, `omarchy:args`).
- Instalado como pacote (`omarchy 4.0.4-1`), com kernel próprio (`linux-omarchy`).

---

## 3. Tema

### Formato
`colors.toml` de cada tema define:
```toml
mode = "dark"
accent = "#89b4fa"
selection = "#45475a"
muted = "#585b70"
background = "#1e1e2e"          # + dark_background, darker_background, lighter_background
foreground = "#cdd6f4"          # + dark_foreground, light_foreground, bright_foreground
red/yellow/orange/green/cyan/blue/magenta/brown (+ variantes bright_*)
```

### Como o tema é aplicado (`omarchy-theme-set`)
1. Monta o tema em `~/.local/state/omarchy/current/next-theme` (tema oficial + overlay do usuário).
2. Gera configs a partir de templates `*.tpl` (`{{ accent }}`, `{{ accent_rgb }}`, `{{ mix a b 30% }}`...).
3. **Troca atômica**: `mv next-theme → theme`, grava `theme.name`.
4. Envia a paleta ao shell por IPC, reinicia/retinge apps conhecidos (`omarchy-theme-set-obsidian`, `-vscode`...).
5. Roda `omarchy-hook theme-set <nome>` (scripts em `~/.config/omarchy/hooks/theme-set.d/`).

### Resolver de cores
`omarchy-theme-color --all` imprime `chave<TAB>valor` de **todas as cores
resolvidas** (aliases, nomes legados, tons derivados, detecção de modo
claro/escuro). É o mesmo resolver usado pelos templates.

### Recomendação para o OmaMovie
- **Ler a paleta com `omarchy-theme-color --all`**: sem dependência de parser
  TOML e com a mesma resolução do Omarchy. Fallback: ler `colors.toml`
  diretamente; fallback final: paleta embutida (o app precisa funcionar fora do Omarchy e em testes).
- **Troca ao vivo**: observar `~/.local/state/omarchy/current/` (o diretório
  `theme` é **substituído** por `mv`, então observar o pai; `theme.name` muda a
  cada troca) com `QFileSystemWatcher`, com debounce. Evita instalar hooks no diretório do usuário.
- Mapear a paleta para tokens semânticos da UI (fundo de painel, seleção,
  acento, texto secundário) num único ponto (`ThemeProvider` exposto ao QML).
- **Nunca tematizar áreas críticas de cor**: preview, scopes, color picker e
  miniaturas mostram a imagem real. O tema afeta só a interface ao redor.
- Fontes: `omarchy-font-current` e o hook `font-set.d`.

---

## 4. Opacidade de janela (crítico para cor)

O Omarchy aplica translucidez a **todas** as janelas:
```lua
o.window(".*", { tag = "+default-opacity" })
o.window({ tag = "default-opacity" }, { opacity = "0.985 0.96" })
```
O próprio Omarchy abre uma exceção para o DaVinci Resolve (`default/hypr/apps/davinci-resolve.lua`):
> "Kept fully opaque: the default translucency distorts colour-critical grading work."

**O OmaMovie precisa da mesma exceção.** Uma janela 96% opaca mistura o
wallpaper no preview e invalida qualquer avaliação de cor.

- O app define um **app_id Wayland estável**: `omamovie` (via `QGuiApplication::setDesktopFileName("omamovie")`).
- Regra necessária:
  ```lua
  o.window("omamovie", { tag = "-default-opacity", opacity = "1 1" })
  ```
- Onde colocar: a curto prazo, documentar para o usuário (config do Hyprland em
  `~/.config/hypr`); a médio prazo, **propor upstream** um `default/hypr/apps/omamovie.lua`, como existe para o Resolve.
- O cliente Wayland não controla a opacidade aplicada pelo compositor.

---

## 5. Gravações de tela

`omarchy-capture-screenrecording` (atalho padrão **ALT+PRINT**) usa o `gpu-screen-recorder`:
```
gpu-screen-recorder ... -k auto -f 60 -fm cfr -fallback-cpu-encoding yes -o <arquivo> -a <áudio> -ac aac
```
- Saída em `${OMARCHY_SCREENRECORD_DIR:-$XDG_VIDEOS_DIR}` (normalmente `~/Videos`).
- Codec automático conforme a GPU (`-k auto`: H.264/HEVC/AV1), **60 fps CFR**, áudio **AAC** (desktop e/ou microfone).
- Opção de webcam sobreposta (picture-in-picture) já gravada no vídeo.

### Recomendações
- **Fonte "Gravações" no MediaPanel**: observa o diretório de vídeos e mostra
  as gravações recentes. É o fluxo mais natural para criadores no Omarchy (gravar → editar).
- Garantir que o caminho de decode aceita exatamente esses arquivos (H.264/HEVC/AV1 + AAC). Usar gravações reais como fixtures de teste (geradas localmente, não commitadas se grandes).
- **Futuro / upstream**: notificação "Editar no OmaMovie" ao parar a gravação
  (`omarchy-notification-send` aceita `--exec`). Hoje o script não tem hook pós-gravação; isso exigiria contribuição ao Omarchy.
- Gravação **separada** da webcam e da tela (duas trilhas) daria mais controle
  na edição do que a webcam já composta. Ideia para propor ao Omarchy ou para gravação integrada no próprio OmaMovie (via PipeWire/portal ScreenCast).

---

## 6. Shell (Quickshell): plugins e IPC

Plugins ficam em `~/.config/omarchy/plugins/<id>/` com um `manifest.json`:
```json
{
  "schemaVersion": 1,
  "id": "io.github.<autor>.<nome>",
  "kinds": ["bar-widget"],
  "entryPoints": { "barWidget": "BarWidget.qml" }
}
```
Tipos (`kinds`): `bar-widget`, `panel`, `overlay`, `menu`, `service`, `bar`.
Instalação por repositório git ou à mão + `omarchy-shell shell rescanPlugins`.
Plugins de terceiros recebem facades com capacidades limitadas.

IPC: `omarchy-shell shell summon|hide|toggle|call <id> ...`, `listPlugins`, `reloadConfig`.

### Recomendações
- **Opcional e posterior**: um plugin `bar-widget` que mostra o progresso de
  export/render em background, para o usuário poder fechar ou minimizar o editor.
- Comunicação app → widget por um canal simples (arquivo de estado em
  `$XDG_RUNTIME_DIR` ou D-Bus). O plugin não deve conter lógica do editor.
- Não é prioridade. O app precisa ser completo sem o shell.

---

## 7. Menu, notificações e atalhos

- **Menu**: `~/.config/omarchy/extensions/omarchy-menu.jsonc` aceita entradas
  com `icon`, `label`, `action`, `when`, `provider` (linhas dinâmicas via
  comando que retorna JSON). Exemplos para o OmaMovie: "OmaMovie › Novo projeto",
  "› Projetos recentes" (provider), "› Editar última gravação".
- **Notificações**: usar o padrão freedesktop (`org.freedesktop.Notifications`
  via D-Bus), que o shell do Omarchy atende. `omarchy-notification-send` é um wrapper conveniente, mas acoplar ao script não é necessário.
- **Lançamento**: `omarchy-launch-or-focus` (foca a janela se já aberta) é o padrão do Omarchy para atalhos de apps.

---

## 8. Drivers instalados pelo Omarchy (o que esperar em cada máquina)

| Fabricante | O Omarchy instala | Implicação |
|---|---|---|
| **Intel** | `vulkan-intel`, `intel-media-driver` (VA-API iHD), `libvpl`, `vpl-gpu-rt` (QSV) | VA-API, Vulkan Video e QSV disponíveis |
| **AMD** | `vulkan-radeon` (VA-API vem com o Mesa) | VA-API e Vulkan Video (RADV) disponíveis |
| **NVIDIA** (com GSP) | `nvidia-open-dkms`, `nvidia-utils`, `libva-nvidia-driver`; envs `LIBVA_DRIVER_NAME=nvidia`, `NVD_BACKEND=direct` | NVDEC/NVENC e Vulkan; VA-API via camada de tradução |
| **NVIDIA** (sem GSP) | Driver 580xx legado | Suporte mais limitado |
| Todos | **Nenhum runtime OpenCL**, nenhum CUDA toolkit | Não depender de CUDA toolkit por padrão (OpenCL foi removido do projeto) |

---

## 9. Empacotamento e distribuição

- **Arch/PKGBUILD** como formato primário (o Omarchy é Arch).
- **OmaStore**: há um hook `omastore.hook` em `post-update.d` nesta máquina. Um
  `omastore.toml` e releases compatíveis permitiriam instalar o OmaMovie pela loja.
- **`.desktop` + MIME**: `omamovie.desktop` (app_id igual), MIME para o formato de projeto e associação "Abrir com" para vídeos.
- **Portais**: `xdg-desktop-portal-hyprland` e `-gtk` estão instalados; seletor de arquivos e ScreenCast via portal funcionam sem código específico do Hyprland.
- **Meta de longo prazo**: propor ao Omarchy o OmaMovie como editor padrão (no
  lugar do Kdenlive), incluindo a regra de opacidade e o `omarchy-theme-set-omamovie`, se fizer sentido.

---

## 10. Regras para o código

- Toda integração com o Omarchy fica num módulo único do app (ex.: `apps/omamovie/src/platform/omarchy/`), **nunca** nas libs.
- Toda leitura de caminho do Omarchy tem fallback; o app funciona em Hyprland sem Omarchy e em testes headless.
- Não escrever em `~/.config/omarchy` sem ação explícita do usuário (ex.: botão "Integrar ao menu do Omarchy").
- Não executar scripts `omarchy-*` no caminho quente; só leituras pontuais (tema, fonte).

---

## Fontes (arquivos locais, Omarchy 4.0.4)

- `/usr/share/omarchy/bin/omarchy-theme-set`, `omarchy-theme-set-templates`, `omarchy-theme-color`, `omarchy-hook`, `omarchy-notification-send`, `omarchy-capture-screenrecording`
- `/usr/share/omarchy/themes/*/colors.toml`, `/usr/share/omarchy/default/themed/*.tpl`
- `/usr/share/omarchy/default/hypr/windows.lua`, `apps/davinci-resolve.lua`, `nvidia.lua`, `bindings/utilities.lua`
- `/usr/share/omarchy/shell/README.md`
- `/usr/share/omarchy/install/omarchy-base.packages`, `install/hardware/{vulkan.sh,nvidia.sh,intel/video-acceleration.sh}`
- `~/.config/omarchy/extensions/omarchy-menu.jsonc`, `~/.config/omarchy/plugins/*/manifest.json`, `~/.config/omarchy/hooks/`
- [Quickshell](https://quickshell.org/)
