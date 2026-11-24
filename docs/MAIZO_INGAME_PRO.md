# MAIZO In-Game PRO (experimental and opt-in)

A new **small EE in-game menu** is available when MAIZO In-Game PRO is
enabled in Settings → Games. GSM video mode forcing is now OPTIONAL:
when GSM is OFF, the menu uses a passive GS write observer that tracks
the frame readout registers without changing the output mode.
Its hotkey is the **existing OPL IGR combo**:
L1 + L2 + R1 + R2 + Start + Select.

When a compatible framebuffer is known, this combo pauses EE game threads
and displays a small 160×126 panel drawn with a compact bitmap font directly
into the game's framebuffer via GS/VIF1. No full-screen framebuffer,
textures, dynamic allocation, filesystem or continuous refresh loop.
RiptOPL/RIPPS2/ORBIT are credited for the low-level GS transfer technique.

## Categories and actual behavior

- **Imagem e video:** small, bounded GS DISPLAY geometry edits (image
  position X/Y and visible display width/height), one expanded preset,
  original restore, and GSM FIELD_fix (distinct from true Deflicker).
  Edits affect the active session while GSM interception is running.
  The units are device/GSM display parameters, NOT guaranteed arbitrary
  pixel-perfect percentages or resampling.
- **Controles:** status/instructions only in this first version. Do not
  change PadEmu's IOP HID modules while a game is running without a safe IPC.
- **Jogo e Cheats:** instructions/status only. Existing PS2RD cheats are
  enabled at launch as configured; on/off individual live cheats are NOT
  wired into the engine by this implementation. Existing IGS shortcut still
  works on IGS-enabled builds, not automatically on all builds.
- **Sistema:** resume, exit to existing IGR path, power off (two presses).

The menu is **disabled by default** because PS2 games can use GS modes and
VIF/DMA paths incompatible with an overlay. When explicitly enabled,
it accepts the IGR hotkey regardless of the GSM mode-selector setting.
With the MAIZO menu OFF, the original OPL IGR remains unchanged. When a safe frame cannot be determined,
the handler refuses to draw and falls back to the original IGR reset.

## Safety and limits

- Hardware tests are still required; the successful cross-compile and host
  state-machine tests are NOT a substitute for real-console validation.
- No driver mappings, cheat engine, network streaming or USB IO in the
  resident in-game code; no ongoing work while the menu is closed.
- A game may repaint the panel's pixels after RESUME. The overlay does
  not reserve 80–320 KiB just to save a copy of the underlying picture.
- Some video formats or games that use DMA MFIFO cannot show the overlay.
- A crash or wrong video mode can still occur; use per-game IGR Mode 6
  to disable hooks in an incompatible title and disable this feature
  globally if needed.
- **Not implemented yet:** universal Deflicker, arbitrary stretch, saved
  in-game presets across boots, live remapping/reconnection, live cheat
  toggling, and screenshot from the menu itself. No false success reports.
- This work cannot claim universal or 90% compatibility without test
  results from actual PS2 hardware.

### IGS variant memory safety

The 77 KB protected EE-core region cannot fit the additional overlay and
IGS screenshot code together on all variants. Builds with `IGS=1` retain
IGS and disable the overlay hook entirely; standard `IGS=0` builds
include MAIZO In-Game PRO. This follows the principle of never enlarging
the low-memory region or placing permanent overlay code in game RAM.

## Filtros de hardware (MAIZO GS Filter, opt-in)

**Imagem e vídeo → Filtros gráficos** oferece controles ON/OFF. Os filtros
são **mutuamente exclusivos** e cada opção só é ativada se a leitura dos
registradores originais do GS confirmar compatibilidade:

- **Deflicker ON/OFF**: é exatamente o modo Smooth. Quando o jogo usa
  480i/576i em **interlace FRAME** com **um único read circuit**, passa a
  usar os dois circuitos sobre o MESMO framebuffer, com deslocamento de
  uma linha e mistura de alpha fixa **0x70**. É filtro vertical de duas
  amostras, não shader nem pós-processamento via CPU.
- **Smooth ON/OFF**: alias amigável para esse Deflicker de hardware.
  Não processa texturas, não remove automaticamente todos os serrilhados.
- **Sharp ON/OFF**: só fica habilitado quando o jogo já usa uma
  composição de dois circuitos no mesmo framebuffer (deslocamento
  vertical de até uma linha). Nesses casos, usa somente o circuito
  **sem deslocamento**. Não inventa nitidez/resolução em jogos que já
  usam um circuito único.
- **Original**: ambos OFF. Restaura os registros PMODE, DISPFB1/2 e
  DISPLAY1/2 como observados pelo GSM do jogo.
- AA1, filtragem bilinear e dithering são indicados como **controlados
  pelo jogo**, e NÃO possuem botões fictícios ON/OFF. Esses efeitos são
  registrados no caminho de desenho (PRIM/TEX1/DTHE) e não equivalem
  aos ajustes do circuito de saída da imagem.

### Proteções

1. Modo padrão OFF, mesmo com o menu In-Game ativado.
2. Não aplica Deflicker em modos progressivos nem em jogos com duas
   imagens independentes nos circuitos de saída.
3. Durante trocas de framebuffer, o hook GSM atualiza o outro circuito,
   evitando misturar frames distintos (ghosting).
4. Em alterações inesperadas do PMODE ou de SetGsCrt, cancela o filtro.
5. Restaura a imagem original ao desligar.
6. Sem carregamento dinâmico, textures, threading adicional ou CPU
   trabalhando por frame: aproveita os writes de GS já interceptados
   pelo próprio GSM.
7. Recurso não aparece em compilações com IGS por falta de memória
   protegida no EE, sem afetar a captura de tela existente.
8. Compatibilidade universal não é possível: se o jogo escrever
   diretamente por uma via que o GSM não intercepta, os filtros não
   estarão cobertos. Testes em consoles reais ainda são necessários.

**Créditos:** PS2SDK `graph_mode.c` (filtro por dois read circuits),
RiptOPL/RIPPS2 (overlay com GS), OPL GSM original.

## Independente do seletor de vídeo GSM (modo de observação)

- **In-Game PRO ON + GSM OFF**: pressionar L1+L2+R1+R2+START+SELECT
  pode abrir o menu, navegar pelas páginas, mostrar diagnósticos e
  acessar as ações existentes continuar/sair/desligar. A saída de vídeo
  não é forçada e as escritas GS/SetGsCrt do jogo são repassadas sem
  modificações pelo observador.
- **GSM ON**: permanece o caminho anterior, incluindo ajustes de
  geometria, FIELD fix, Deflicker/Sharp/Smooth e saída 480p>480i
  quando suas condições de hardware forem satisfeitas.
- Sem GSM, os controles que exigem força de vídeo aparecem como
  REQUER GSM e não são executados. AA1, Bilinear e Dither mantêm a
  política de só ativar se um adaptador de desenho for registrado
  com suporte real ao jogo. Não simulamos melhorias.
- O menu continua usando o GS PATH2/VIF1 em uma janela pequena,
  pausando outras threads. Se não houver framebuffer compatível,
  não tenta adivinhar o endereço: retorna ao caminho antigo de IGR.
- O observador só registra PMODE, SMODE2, DISPFB1/2 e DISPLAY1/2,
  sem alterar os dados escritos. **Importante:** ele reutiliza
  a infraestrutura de breakpoint GS do GSM, mas NÃO o GSM como
  seletor/conversor de resolução. Não é uma instrumentação inteiramente
  separada do código GSM e ainda pode conflitar com jogos que alteram
  handlers de debug. Teste no PS2 físico é indispensável.
