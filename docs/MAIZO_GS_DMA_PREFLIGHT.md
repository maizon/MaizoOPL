# MAIZO GS DMA pre-submit — proteção para jogos e limites de compatibilidade

## Estado real

O PS2 possui **três caminhos diferentes** até o GS: PATH1 (VU1 XGKICK),
PATH2 (VIF1 DIRECT) e PATH3 (GIF DMA). O GSM do OPL intercepta escritas
nos registradores privilegiados de vídeo, **não** todos os pacotes que
passam pelos três caminhos gráficos. Não existe, neste commit, uma
interceptação universal confiável dos três caminhos.

**Nenhum percentual de compatibilidade (inclusive 90%) foi estabelecido.**
Não ativar filtros in-game indiscriminadamente sem testes em PS2 real.

## O que foi implementado de verdade

`ee_core/include/maizo_gs_dma_preflight.h` é um adaptador de **pré-envio**
para código de renderização que colabora com o MAIZO. Ele transforma
um pacote **antes** de seu envio no canal 2 (PATH3), desde que:

- O buffer pertence ao chamador e está alinhado a 16 bytes, em memória
  normal de EE; não pode estar em scratchpad ou em região executável.
- A DMA ainda não começou; o chamador é responsável por confirmar isso.
- O tamanho é múltiplo de 16 bytes e não ultrapassa 1 MiB.
- O pacote é completo (GIF EOP) e possui sintaxe validada em duas passagens.
- No modo DMAC chain, somente CNT/END com payload inline e completo
  são aceitos. Qualquer REF/REFE/NEXT/REFS/CALL/RET ou SPR é rejeitado
  sem seguir endereços de jogos ou modificar nenhum byte.
- Em uma cadeia, **todos** os trechos são validados antes de editar o primeiro.
- O chamador tem de sincronizar a cache EE (por exemplo com
  `FlushCache(0)`) antes de iniciar a DMA, e garantir exclusividade.

Somente buffers submetidos **voluntariamente pelo chamador** são
processados. Nenhum hook universal é instalado em jogos comerciais.

### Correções do parser GIF

- Rejeita o modo FLG=3 reservado.
- Suporta várias listas completas EOP concatenadas em um mesmo buffer,
  ao invés de alterar só a primeira silenciosamente.
- O AA1 em PRMODE não usa bits PRIM inexistentes em PRMODE: acompanha
  o tipo de primitiva conhecido da última PRIM.
- A ativação de DTHE exige o formato 16-bit **do contexto de desenho
  ativo**, não apenas que algum contexto use 16-bit.
- Exige stream completo terminado em EOP, rejeitando streams parciais.
- Testes validam falhas sem modificações parciais, PATH1/PATH2 ignorados,
  PATH3 pré-envio, DMA ativa/buffer não pertencente ao chamador,
  inserções bilinear/AA1/dither e DMAC chain CNT/END.

## Por que não ativar 90% já?

Um jogo real pode enviar GIF por PATH1 ou PATH2 a partir de VU1,
VIF microcódigo, DMA chain com referências externas e scratchpad.
Intervir em pacotes que já estão em voo é especialmente perigoso.
Uma interceptação genérica no registrador do canal GIF também entra em
conflito com o debug breakpoint já usado pelo GSM.

**Próximo componente necessário:** um hook por engine/jogo, instalado
antes do envio e validado pela compatibilidade de cada jogo; idealmente
um loader que possa escolher dispositivos e desativar a implementação
automaticamente se o protocolo gráfico não for reconhecido. Para
cobertura ampla seria necessário interceptar e reconstruir *todos* os
caminhos com sincronização DMAC/VIF/VU, inviável prometer que funcione
sem perda de desempenho ou incompatibilidades.

**Não há botões ON/OFF ativos de AA1, bilinear, dithering e scanlines em
todos os jogos.** É proposital: não anunciar funcionamento inexistente.

Referências: `ps2tek` (GIF/GS/DMA); documentação PS2SDK e
artigos de programação sobre PATH1/PATH2/PATH3.

## Adaptação PATH2 VIF1 DIRECT

A API `mzGsDmaPatchVif1Direct` aceita somente um buffer exclusivamente
controlado pelo chamador contendo **3 VIF NOP + 1 DIRECT (0x50)** em um
cabeçalho de 16 bytes, seguido de exatamente o número de QWs do payload
GIF. Valida os códigos VIF e o tamanho antes de reescrever o GIF. Rejeita
DIRECTHL, UNPACK, MSCAL, cadeias arbitrárias, DMA ativa e buffers sem dono.
Isso dá suporte **limitado** a um subconjunto conhecido do PATH2, mas
não intercepta todos os jogos nem o PATH1 via XGKICK.
