# MAIZO OLED Ultimate

O tema interno usa a biblioteca MAIZO e mantém os módulos de armazenamento,
configuração e inicialização do OPL. **O ELF ainda precisa de uma execução de CI
bem-sucedida; não há validação em PS2 real ou emulador.**

## Biblioteca e navegação

As abas TODOS, PS2, PS1, APPS, FAVORITOS e CONFIGURAÇÕES são alternadas com L1/R1.
A biblioteca agrega as fontes visíveis e inicializadas, ordena títulos e elimina
cópias com a mesma identidade na mesma plataforma. PS1 e PS2 nunca são unidos
apenas por compartilharem um identificador. Aplicativos são identificados pelo
caminho completo, não pelo nome do ELF. Em cópias PS2, a primeira fonte na ordem
dos módulos é mantida de forma determinística. Os temas externos continuam
permitindo a navegação original por dispositivo.

Cada entrada conserva o módulo e o item original. A confirmação usa os callbacks
de inicialização existentes e o bloqueio parental. Não há inicializadores
alternativos nem bibliotecas de demonstração. Refresh/hotplug invalida o catálogo;
seleções são restauradas por identidade, sem reutilizar referências removidas.
Alternar abas e navegar não volta a consultar o armazenamento.

Controles do tema interno:

| Controle | Ação |
| --- | --- |
| L1 / R1 | Aba anterior / seguinte |
| Esquerda / direita | Item anterior / seguinte |
| Cima / baixo | Alternativa para navegar na biblioteca |
| X | Confirmar/iniciar no mapeamento padrão |
| Círculo | Voltar no mapeamento padrão |
| Triângulo | Painel de opções: iniciar, configurações disponíveis, informações, favorito, atualizar fonte |
| Quadrado | Adicionar/remover favorito e gravar imediatamente |
| Start | Abrir configurações |
| Select | Atualizar fontes e iniciar fontes configuradas em modo manual |
| L2 / R2 | Primeiro / último item |

A troca X/Círculo configurada no OPL é respeitada. No painel de configurações,
esquerda/direita alternam categorias e ações; cima/baixo percorrem o foco.

## Favoritos

Favoritos usam chaves `maizo_f<plataforma>_<hash>` em `conf_opl.cfg`. O hash FNV-1a
64 bits é estável; o valor também guarda a identidade completa e é comparado na
leitura. Não há banco de dados separado. Erros de gravação exibem a mensagem
existente de erro e restauram o estado anterior na memória. Remover um dispositivo
não apaga seus favoritos. `maizo_animations` controla as animações persistentes.

## Renderização e memória

- Paleta em `include/maizo_oled.h`; preto padrão, superfícies discretas e azul de foco.
  As cores existentes de configuração continuam sendo lidas. A marca MAIZO é
  desenhada sobre páginas/transições e nos diálogos de configuração; diálogos
  críticos continuam com prioridade.
- Até cinco capas, seleção central maior, título, plataforma, fonte e posição.
  O carrossel agora continua circularmente na primeira/última capa; bibliotecas
  com 1 a 4 itens não repetem capas e mantêm a seleção no centro.
  Imagens ajustadas dentro do cartão sem distorção ou recorte. A largura segue a
  correção de 16:9 do OPL; coordenadas virtuais usam os modos/overscan existentes.
- Deslocamento discreto dos vizinhos, escala do foco e indicador animado de aba.
  O item selecionado permanece centralizado. “Animações OLED”, em GERAL, desativa
  a animação do dashboard. As transições originais de telas do OPL são mantidas.
- Ícones MAIZO originais para plataformas, apps, favoritos, configurações, USB,
  HDD, rede, Bluetooth, controles, vídeo, áudio, informação, busca, biblioteca,
  voltar e confirmar, desenhados com linhas do GS. Nenhum SVG em execução,
  textura adicional ou recurso proprietário do Aurora.
- Falhas de alocação dos índices ART não derrubam o dashboard: o item mantém a
  capacidade de inicialização e recebe a capa substituta.
- Cache ART COV compartilhado, dez entradas e leitura assíncrona já existente.
  A seleção tem prioridade; texturas não usadas no quadro são descartadas quando
  o cache excede 8 MiB de dados decodificados. Texturas usadas no quadro são
  protegidas; esse orçamento pode ser excedido temporariamente por elas.
- PNG externos têm leitura limitada ao tamanho do arquivo, limite comprimido de
  8 MiB e validação de dimensões/tamanho GS. Erros libpng e falhas de alocação
  liberam pixels, paletas e buffers temporários. PNG de paleta de 4 bits com
  largura ímpar são expandidos para 8 bits para preservar a última coluna;
  imagens ausentes/inválidas usam
  a capa substituta. Isso não equivale a carregar todas as capas na VRAM.

## Configurações existentes

O painel agrupa GERAL, VÍDEO, ÁUDIO, CONTROLES, PADEMU, BLUETOOTH,
ARMAZENAMENTO, REDE, PS1 e SOBRE. Abre os formulários reais, sem opções fictícias.
Armazenamento e PS1 abrem a configuração original com foco no campo correspondente;
os demais campos originais continuam acessíveis no formulário. Os formulários
receberam superfície OLED, foco consistente, rolagem automática e barra de posição.

PadEmu, macros, gerente Bluetooth e diagnóstico Twin continuam presentes nas builds
com PADEMU. As builds sem PADEMU identificam explicitamente a indisponibilidade.
Informações do jogo permanecem acessíveis no painel de opções. Configurações
individuais PS2, VMC, GSM e cheats usam os menus originais. PS1 usa o detector de
VCD/ART e o carregamento POPStarter existentes, incluindo mensagens sobre arquivos
externos ausentes ou inválidos. HID/Twin e os drivers Bluetooth não foram substituídos.

## Verificação e CI

Verificações locais realizadas:

- Testes HID e Twin com `-Wall -Wextra -Werror`.
- `tests/maizo_library_test.c`: deduplicação, origem de execução, todas as abas,
  carrossel circular sem duplicatas nas bibliotecas de 0 a 7 jogos,
  favoritos, posição por aba, limpeza/hotplug e 12 mil itens, com ASan/UBSan.
- `tests/texture_host_test.py`: decodificador real PNG/libpng com armazenamento GS
  substituído apenas no teste, pixels RGBA/paletas, truncamento, CRC inválido,
  dimensões/entrada excessivas e limpeza com ASan/UBSan. Em Termux use
  `CC=clang python tests/texture_host_test.py`; em Linux, o compilador padrão.
- Análise de sintaxe nativa com headers reais PS2SDK/gsKit, com PADEMU ligado e
  desligado. **Não é compilação EE nem verificação do link ou de símbolos do ELF.**
- Sintaxe YAML dos workflows e `git diff --check`.

`.github/workflows/compilation.yml` instala PyYAML 6.0.3, confere ferramentas SDK,
usa checkout completo, separa limpeza de compilação, preserva as variantes/debug e
publica ELF como `OPNPS2LD`. Um release sem ELF falha na etapa de upload. O log da
build principal é configurado como artefato `MAIZO-build-log`, inclusive em falhas.
Os testes nativos estão em `.github/workflows/maizohid-tests.yml`.

No primeiro envio deste desenvolvimento, o Actions falhou antes de iniciar etapas:
[execução 42522561403](https://github.com/maizon/MaizoOPL/actions/runs/42522561403).
Repetir o job produziu novamente zero etapas, logs 404 e nenhum artefato. A causa
externa específica ainda depende da anotação da execução; não foi presumida como
erro de código ou faturamento. Sem runner executando, não há ELF novo validado.

Pendências de validação: build/link completos pelo Actions, inspeção dos símbolos
do ELF e execução em FAT/Slim, PS1/PS2/ELF, USB/SMB/HDD, HID/Twin/Bluetooth,
CRT/overscan, 480i/576i/240p/288p e 4:3/16:9. A interface não deve ser anunciada
como compatível com hardware real antes desses testes.

## Referências visuais

Foram examinados [Tabler Icons](https://github.com/tabler/tabler-icons), MIT, e
[flat-gamepad-icons](https://github.com/RobTheFiveNine/flat-gamepad-icons),
CC-BY-SA-4.0. Os novos ícones são desenhos próprios por primitivas; nenhum arquivo
desses repositórios foi incorporado. Os símbolos de botões permanecem os do OPL.
