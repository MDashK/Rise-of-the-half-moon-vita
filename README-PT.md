# Rise of the Half Moon (offline)

Os 8 jogos Google Doodle **Rise of the Half Moon** (outubro 2024 a maio 2025), para jogar sem internet:

- **PC (Windows)** — app C# / WPF que corre os jogos originais (HTML5) num WebView2, servidos a partir de uma cópia local.
- **PS Vita** — versão nativa em C++20 + SDL2: as regras, a IA e os ecrãs foram reimplementados a partir do
  código original; gráficos, sons, vídeo e textos são os originais, descarregados pela própria app.

Port by MDashK / AI assisted by Claude.

> Os jogos (gráficos, música, sons, vídeo e textos) são © Google LLC e **não estão incluídos** neste repositório
> nem nas releases: as duas apps descarregam-nos dos servidores da Google na primeira utilização.
> Projeto não oficial, sem fins comerciais, para uso pessoal e preservação.

## Releases

| Plataforma | Ficheiro | Instalação |
|---|---|---|
| Windows 10/11 (x64) | `RiseHalfMoon-PC-v<versão>-win-x64.zip` | Extrair `RiseHalfMoon.exe` para uma pasta com permissão de escrita (não em `Program Files`) e abri-lo |
| PS Vita (HENkaku/Ensō) | `RiseHalfMoon-PSVita-v<versão>.vpk` | Instalar com o VitaShell |

Na primeira utilização, carregar em **Descarregar / atualizar jogos** (é preciso internet só dessa vez).

| Jogo | Pasta original (`https://www.google.com/logos/2024/moon/…`) |
|---|---|
| Outubro 2024 | `r3/` |
| Novembro 2024 | `novr2/` |
| Dezembro 2024 | `december-r4/` |
| Janeiro 2025 | `moon_january-r1/` |
| Fevereiro 2025 | `moon_february-r4/` |
| Março 2025 | `moon_march-rc2/` |
| Abril 2025 | `moon_april-rc2/` |
| Maio 2025 | `moon_may-rc3/` |

---

## PC (Windows)

### Utilização

- **Idioma**: português (PT e BR), inglês, espanhol, francês, alemão, italiano e neerlandês. Escolher no
  menu muda logo a interface e a língua dos jogos. Na primeira vez usa a língua do Windows, se for uma destas.
- **?** abre a caixa *About* (versão, créditos, direitos e licenças).
- Clicar num jogo para jogar. Na barra do jogo: **Menu**, **Recomeçar** e **Ecrã inteiro (F11)**.
- **Descarregar / atualizar jogos** guarda tudo na pasta `Jogos/` ao lado do `.exe`
  (os ficheiros que já existem não são descarregados de novo).

### Como funciona

- `Jogos/` guarda uma cópia exata de cada ficheiro necessário, organizada por URL
  (`Jogos/www.google.com/logos/2024/moon/...`, fontes em `Jogos/fonts.gstatic.com/...`, etc.).
- Cada jogo abre no WebView2 com o endereço original, mas a app **intercepta todos os pedidos** e responde a
  partir de `Jogos/`. Nada sai para a internet:
  - a telemetria do Google (`gen_204`, `async/ddllog`) recebe uma resposta vazia;
  - qualquer outro endereço é bloqueado (links externos, como os de partilha, abrem no browser normal).
- Pontuações, progresso e a língua escolhida ficam em `%LOCALAPPDATA%\RiseHalfMoonOffline`.

### Argumentos de linha de comandos

| Argumento | Efeito |
|---|---|
| `--data <pasta>` | Usa outra pasta de jogos (por omissão `Jogos` ao lado do `.exe`) |
| `--download` | Descarrega tudo sem abrir janela e termina (registo em `download.log`) |
| `--offline` | Nunca usa a internet, nem para completar ficheiros em falta |
| `--play <1-8>` | Abre logo o jogo indicado |

### Compilar

Requisitos: .NET 8 SDK e Microsoft Edge WebView2 Runtime (já vem com o Windows 11 e com a maioria dos Windows 10).

Abrir `RiseHalfMoon.sln` no Visual Studio 2022 e carregar em F5, ou:

```bash
dotnet run --project src/RiseHalfMoon
```

Release — um único `RiseHalfMoon.exe` (~70 MB, não precisa do .NET instalado) e sem os jogos, que a app descarrega:

```bash
dotnet publish src/RiseHalfMoon -c Release -r win-x64 --self-contained true -p:IncludeGames=false -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:EnableCompressionInSingleFile=true -p:DebugType=none -o publish
```

Se existir uma pasta `Jogos/` na raiz do repositório, as builds de desenvolvimento copiam-na para junto do
`.exe` (útil para testar sem descarregar).

---

## PS Vita

Versão nativa (C++20 + SDL2). VPK com o Title ID `RHMOON001`.

- Os 8 meses são jogáveis: introdução em vídeo, tutorial, campanha, IA e os 4 wildcards de cada mês.
- Interface só por toque, com botões grandes. Menu horizontal; o jogo pode ser horizontal ou vertical
  (o vertical é igual ao do telemóvel), com opção para rodar 180°.
- Menu e jogos em 8 línguas (as mesmas do PC); mudar de língua no menu muda tudo na hora.
- Botão **?**: caixa *About* com a versão (sobe automaticamente a cada compilação), créditos e licenças.
- Dados em `ux0:/data/rise_half_moon/`: jogos em `games/`, `settings.json`, progresso em `save_<mês>.json`.

### Compilar (WSL com VitaSDK)

Dependências (vdpm): `sdl2`, `curl-mbedtls` (instala `mbedtls`), `zlib`, `zstd`.

```bash
cd vita
export VITASDK=/usr/local/vitasdk PATH=/usr/local/vitasdk/bin:$PATH
cmake -S . -B build-vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-vita -j8
```

O VPK fica em `vita/build-vita/RiseHalfMoon.vpk`. Mais detalhes (dados extraídos, diagnóstico) em
[`vita/README.md`](vita/README.md).

---

## Créditos e licenças

- "Rise of the Half Moon" © Google LLC. Google e Google Doodle são marcas da Google LLC.
- PC: Microsoft Edge WebView2 SDK (BSD-3-Clause), .NET / WPF (MIT).
- PS Vita: SDL2 (zlib), libcurl (curl), Mbed TLS (Apache 2.0), zlib, Zstandard (BSD), stb (domínio público),
  certificados CA da Mozilla (MPL 2.0), fonte Josefin Sans (SIL OFL 1.1), VitaSDK.
- PlayStation e PS Vita são marcas da Sony Interactive Entertainment Inc. Microsoft, Windows e Microsoft Edge
  são marcas do grupo Microsoft. Projeto sem qualquer ligação ou apoio destas empresas.
