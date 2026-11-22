# Cheats integrados ao Maizo Supreme

Coloque o banco CheatDevice em `/Cheats/CheatDatabase.zip` na raiz de um dispositivo. Ordem: mass0 a mass9 (FAT32/exFAT e demais BDM montados), mc0, mc1, SMB conectado (`smb0:\\Cheats\\CheatDatabase.zip`). O primeiro ZIP aberto é o único consultado. SMB usa a sessão configurada no OPL; conecte-a primeiro se estiver em modo manual. OPL não grava nem extrai o ZIP no dispositivo.

O ZIP pode ter TXT CheatDevice (blocos com títulos entre aspas) ou arquivos TXT/CHT identificados pelo ID exato, por exemplo SLUS_123.45.cht. Deflate e Stored são suportados; ZIP64, criptografia, arquivos corrompidos e volumes excessivos são recusados. A busca por ID tem prioridade. Bancos sem ID requerem nome correspondente, região compatível se informada, e confirmação humana para não aplicar códigos de outra versão. Duplicatas ambíguas não são mescladas. O leitor usa fluxo de leitura e preserva somente a seção encontrada, com CRC de integridade.

Quadrado abre os detalhes, imagens ART/<ID>_COV.png, ART/<ID>_SCR.png e ART/<ID>_SCR2.png (se existentes). Configurar jogo > Cheats do jogo abre a lista; selecione manualmente, START grava, Voltar cancela. Mastercodes ficam automáticos. O código usa o PS2RD existente, não substitui o núcleo pelo CheatDevice. Mapas de valores ($...), códigos criptografados e recursos específicos de outro engine não são garantidos; grupos não resolvidos são ignorados. Seleções são gravadas por jogo e vinculadas ao CRC do bloco selecionado. CHT antigo continua disponível quando não existe ZIP. Sem banco ou seleção válida não aplica cheats.

Referência do formato: https://github.com/root670/CheatDevicePS2 e https://israpps.github.io/CheatDevicePS2/Documentation/Cheats.html.

Validação física necessária: combinação de arquivos reais, MC nativo, FAT32, exFAT, SMB, versões de jogos, renderização CRT e limites do PS2RD.
