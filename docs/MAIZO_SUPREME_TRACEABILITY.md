# MAIZO SUPREME — Matriz de rastreabilidade

Esta matriz documenta a integração da camada Supreme com as rotinas reais do MaizoOPL. **Compilar não valida o PS2 real.** Menus cujos controles ainda utilizam a estrutura `UIItem` existente são redesenhados pelo renderizador compartilhado em `src/dia.c`; o backend e a persistência permanecem nos callbacks originais.

## Equivalência de funções

| Recurso | Acesso Supreme | Backend existente | Persistência e proteção | Validação |
|---|---|---|---|---|
| Dashboard Todos/PS2/PS1/APPS/Favoritos | L1/R1 | `menuDashboardSlot`, `maizoLibraryFilter` | Identidade e seleção por plataforma | Testes host biblioteca |
| Capa e carrossel | Dashboard | `drawMaizoCarousel`, `getGameImageTexture` | Cache ART original, 5 slots visíveis | Compilação; PS2 pendente |
| Favoritos | Triângulo > Favorito | `dashboardToggleFavorite` | `maizo_f*` por plataforma/identidade; gravação real | Host biblioteca; hardware pendente |
| Ordenação | START > JOGOS > Organizar | `maizoLibrarySortMode` | `maizo_sort_mode`: nome, plataforma, origem | Testes host biblioteca |
| Capas ligadas/desligadas | START > INTERFACE > Capas | `drawMaizoCarousel` | `maizo_cover_mode` | Integração CI; hardware pendente |
| Animação | START > INTERFACE > Animações | `menuDashboardFocus`, `guiShow` | `maizo_animation_mode`: off/suave/completa | Compilação; FPS não medido |
| Aparência e vídeo global | INTERFACE / VÍDEO | `guiShowUIConfig`, `guiConfirmVideoMode` | Chaves OPL existentes, reversão de vídeo | Compilação; hardware pendente |
| Áudio | ÁUDIO | `guiShowAudioConfig` | Configuração OPL original | Compilação; hardware pendente |
| Controles PS2 | CONTROLES | `guiShowControllerConfig` | Preferências OPL originais | Compilação; hardware pendente |
| USB HID | DIAGNÓSTICO > USB | `guiGameShowUsbDiagnostics` | IOP `ds34usb` e consulta RPC | Parser host; PS2 pendente |
| Bluetooth | CONTROLES > Bluetooth | `guiGameShowBluetoothManager` | Pareamento original | Compilação; hardware pendente |
| PadEmu global/por jogo | CONTROLES / jogo | `guiGameShowPadEmuConfig` | Configuração original e `#ifdef PADEMU` | Variantes CI |
| PadMacro | Configurações do jogo | `guiGameShowPadMacroConfig` | Configuração original | Variantes CI |
| Compatibilidade | Configurações do jogo | `guiGameShowCompatConfig` | Modos 1–6 originais, ID, DMA etc. | Compilação |
| GSM | Configurações do jogo | `guiGameShowGSConfig` | Perfil original por jogo | Compilação |
| Cheats | Configurações do jogo | `guiGameShowCheatConfig` | Chaves originais | Compilação |
| VMC | Configurações do jogo | `guiGameShowVMCMenu` | Arquivos VMC não migrados | Compilação |
| OSD | SISTEMA / jogo | `guiGameShowOSDLanguageConfig` | Global e específico | Compilação |
| Armazenamento | ARMAZENAMENTO | `guiShowStorageConfig` | BDM, HDD, USB, MX4SIO e iLink condicionais | Compilação |
| Rede e SMB | REDE | `guiShowNetConfig` | Porta/endereço/usuário existentes | Compilação |
| Atualizações de compatibilidade | REDE | `guiShowNetCompatUpdate` | Backend de atualização existente | Compilação |
| Proteção parental | SISTEMA | `menuCheckParentalLock`, `guiShowParentalLockConfig` | Senha e regras OPL originais | Compilação |
| Renomear/excluir | Menu do jogo/app | `menuRenameGame`, `menuDeleteGame` | Permissão de escrita e confirmação | Compilação |
| Aplicativos ELF | APPS | `execCross/execCircle` e `appGetItemPath` | Caminhos originais | Compilação |
| Diagnóstico de sistema | DIAGNÓSTICO | `guiShowSupremeDiagnostics` | Somente dados realmente presentes | Compilação |
| Sobre e créditos | SISTEMA > Sobre | `guiShowAbout`, `diaAbout` | Créditos OPL preservados | Compilação |

## Assets e componentes

- **Ícones:** subconjunto de Tabler Icons, licença MIT. SVGs originais e LICENSE em `assets/maizo/tabler/`. Vetores simplificados em `include/maizo_tabler_strokes.h`, usados por `maizoDrawIcon`. O GS desenha primitivas sem interpretador SVG.
- **Componentes:** `maizoDrawHeader`, `maizoDrawRow`, `maizoDrawPanel`, `maizoDrawSeparator`, diálogo universal, teclado, notificações, confirmação.
- **Aparência:** tokens de cor em `include/maizo_oled.h`, mantido tema clássico quando selecionável.
- **Logo:** tipografia MAIZO transparente temporária. O PNG original aprovado em MaizoWii **não foi encontrado** e NÃO foi inventado como se fosse o mesmo asset.
- **Fonte:** renderizador de fontes OPL preservado; fonte externa licenciada ainda não integrada.
- **Idioma:** textos novos ainda precisam de integração abrangente ao mecanismo de localização, além dos textos originais preservados.

## Critérios ainda pendentes de validação real

- Microten 0079:0006 passando de enumerado até relatórios HID/PadEmu e jogabilidade durante uma sessão longa.
- DS3/DS4 e Twin USB no hardware; VID/PID não demonstram recepção de relatórios.
- Boot de PS2/PS1/POPStarter, VMC, GSM, Bluetooth, SMB, iLink e MX4SIO em consoles reais.
- Desempenho real em 480i, 576i e outros modos: sem contagem de FPS/prazos mensurados não alegar 60/50 Hz sustentados.
- Completar localização e importar logo PNG original/licenciada.
- Auditar visualmente todas as condições excepcionais e traduções em CRT 4:3.

Não usar resultados de Actions como prova de funcionamento no console. Artefatos do commit final devem ser rastreados pelo SHA.
