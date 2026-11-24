# Cheats integrados ao Maizo Supreme

Coloque o banco CheatDevice em `/Cheats/CheatDatabase.zip` na raiz de um dispositivo. Ordem: mass0 a mass9 (FAT32/exFAT e demais BDM montados), mc0, mc1, SMB conectado (`smb0:\\Cheats\\CheatDatabase.zip`). O primeiro ZIP aberto é o único consultado. SMB usa a sessão configurada no OPL; conecte-a primeiro se estiver em modo manual. OPL não grava nem extrai o ZIP no dispositivo.

O ZIP pode ter TXT CheatDevice (blocos com títulos entre aspas) ou arquivos TXT/CHT identificados pelo ID exato, por exemplo SLUS_123.45.cht. Deflate e Stored são suportados; ZIP64, criptografia, arquivos corrompidos e volumes excessivos são recusados. A busca por ID tem prioridade. Bancos sem ID requerem nome correspondente, região compatível se informada, e confirmação humana para não aplicar códigos de outra versão. Duplicatas ambíguas não são mescladas. O leitor usa fluxo de leitura e preserva somente a seção encontrada, com CRC de integridade.

Quadrado abre os detalhes, imagens ART/<ID>_COV.png, ART/<ID>_SCR.png e ART/<ID>_SCR2.png (se existentes). Configurar jogo > Cheats do jogo abre a lista; selecione manualmente, START grava, Voltar cancela. Mastercodes ficam automáticos. O código usa o PS2RD existente, não substitui o núcleo pelo CheatDevice. Mapas de valores ($...), códigos criptografados e recursos específicos de outro engine não são garantidos; grupos não resolvidos são ignorados. Seleções são gravadas por jogo e vinculadas ao CRC do bloco selecionado. CHT antigo continua disponível quando não existe ZIP. Sem banco ou seleção válida não aplica cheats.

Referência do formato: https://github.com/root670/CheatDevicePS2 e https://israpps.github.io/CheatDevicePS2/Documentation/Cheats.html.

Validação física necessária: combinação de arquivos reais, MC nativo, FAT32, exFAT, SMB, versões de jogos, renderização CRT e limites do PS2RD.

Seleções novas identificam também o conteúdo RAW de cada grupo, evitando escolher outra cheat com nome duplicado. O limite de códigos e hooks é aplicado antes de salvar e novamente antes de iniciar. L1/R1 avançam páginas; a lista mostra quantas opções estão ativadas. O firmware do motor também rejeita listas maiores que seus buffers.

O leitor agora informa separadamente erro de leitura, estrutura ZIP, compactação, CRC, memória ou limite. O limite de leitura do arquivo é 512 MiB, com orçamento de 256 MiB de TXT descompactado e 256 KiB por seção de jogo; a leitura continua em fluxo. ZIP64 completo ainda não é aceito.

No In-Game PRO experimental, Jogo e Cheats permite pausar/retomar os códigos RAW previamente carregados, somente após confirmar que o hook PS2RD foi instalado. Pausar não desfaz valores já escritos e não carrega novos grupos do ZIP durante a partida.

Mapas `[nome]` e referências `$nome` com prefixo hexadecimal par agora geram opções nomeadas mutuamente exclusivas no frontend. Suporta uma referência por grupo; mapas inválidos, acima do limite ou desconhecidos não são executados. As escolhas são vinculadas aos códigos RAW resolvidos. A seleção individual ao vivo ainda é independente dessa expansão.

O modo individual experimental pré-carrega até 8 grupos e 48 linhas de escrita RAW simples (tipos 0/1/2) com Master Code de hook 9. A seleção muda somente uma máscara, preservando o Master Code e sem reescrever código enquanto o jogo está pausado. Se algum grupo inicialmente selecionado não cabe ou usa comandos complexos, permanece no modo legado de pausa global. Salvar uma seleção vazia na tela de cheats autoriza o catálogo compatível para uso durante o jogo. Não há leitura de ZIP durante a partida.

SMB: arquivos ZIP compactados de até 4 MiB usam leitura sequencial em blocos de 2 KiB, com nova abertura e blocos de 1 KiB/512 bytes se necessário. O tamanho vem de getstat ou seek64/seek. A cópia compactada temporária é liberada antes do jogo. Bancos maiores ou falta de heap usam leitura em fluxo com blocos de 1 KiB. O diagnóstico de I/O mostra etapa, offset, tamanho solicitado e retorno do driver.

Live catalog: RAW 7 boolean operations and D/E conditionals are accepted only when aligned, within game RAM and wholly contained in one group. E is normalized to D before launch. Types with continuation words, dynamic pointers and global conditions remain on the legacy path; no universal live compatibility claim.

The experimental loader also preloads a bounded names-only preview for games without an approved live catalog. This is read-only and does not execute master hooks or apply cheats. It uses the existing ZIP/SMB reader before boot, not file I/O while the game is paused. Approved compatible groups retain individual toggles. A failed live catalog does not disable the legacy selected codes.
