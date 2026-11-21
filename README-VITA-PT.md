# Rise of the Half Moon — PS Vita

Versão nativa (C++20 + SDL2) dos jogos "Rise of the Half Moon". Os jogos originais são HTML5 e não
correm na Vita, por isso as regras, a IA e os ecrãs foram reimplementados a partir do código original.
Os gráficos, sons, vídeo e textos são os originais, descarregados pela própria app.

- VPK: `build-vita/RiseHalfMoon.vpk` (Title ID `RHMOON001`)
- Dados do utilizador: `ux0:/data/rise_half_moon/` (jogos em `games/`, `settings.json`, `save_<mês>.json`)
- Interface só por toque: menu horizontal; o jogo pode ser horizontal ou vertical (com opção "Flip")
- Versão no *About*: `v1.xx`, sobe 0,01 a cada compilação (contador em `build_number.txt`)

## Estado

Os 8 meses são jogáveis: vídeo de introdução, tutorial, campanha, IA e os 4 wildcards de cada mês.

| Mês | Wildcards |
|---|---|
| Outubro 2024 | Hunter Moon, Leonids, Scorpio, Super Moon (motor antigo: baralho finito, uma carta por turno) |
| Novembro 2024 | Winter Solstice, Geminids, Sagittarius, Beaver Moon (motor antigo) |
| Dezembro 2024 | Capricorn, Long Night Moon, Light of Mars, Quadrantids |
| Janeiro 2025 | Aquarius, Light of Venus, Moon Apogee, Wolf Moon |
| Fevereiro 2025 | Pisces, Snow Moon, Total Lunar Eclipse, Equinox |
| Março 2025 | Aries, Worm Moon, Solar Eclipse, Lyrids |
| Abril 2025 | Taurus, Pink Moon, Eta Aquarids, Close Approach |
| Maio 2025 | Gemini, Flower Moon, Moon at Perigee, Hercules Cluster |

## Compilar (WSL com VitaSDK)

```bash
export VITASDK=/usr/local/vitasdk PATH=/usr/local/vitasdk/bin:$PATH
cmake -S . -B build-vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-vita -j8
```

Dependências (vdpm): `sdl2`, `curl-mbedtls` (instala `mbedtls`), `zlib`, `zstd`.

## Dados extraídos dos jogos originais

`tools/extract_month.py` lê o JavaScript original (formatado com jsbeautifier) e gera `assets/data/<mês>.json`:
tabuleiros e campanha, frames dos sprites, animações, sons, textos da introdução, wildcards e o CSS (cores).
Precisa de Python 3 com `py_mini_racer`:

```bash
python tools/extract_month.py <jogo.pretty.js> <mês> assets/data/<mês>.json <pasta-com-py_mini_racer>
```

Suporta os dois formatos do jogo original (outubro/novembro e dezembro a maio).

## Diagnóstico

Um ficheiro `ux0:/data/rise_half_moon/debug.txt` com estas palavras ativa:

| Palavra | Efeito |
|---|---|
| `trace` | Mostra no ecrã os últimos eventos do jogo e grava-os em `trace.txt` |
| `autoplay` | O jogador também é controlado pela IA (para testar campanhas inteiras) |
| `fast` | Tempo 4x mais rápido |
| `allwild` | Todos os wildcards do mês desbloqueados desde o início |
| `game=<mês>` | Abre logo esse jogo (`oct`, `nov`, `dec`, `jan`, `feb`, `mar`, `apr`, `may`) |
| `reveal` | Mostra a animação de um wildcard novo ao entrar na campanha |
| `nv21` | Ordem de cor antiga no vídeo (só para o Vita3K, que ignora a ordem U/V) |
