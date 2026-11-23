# MAIZO Store v2 — homebrews GitHub gratuitamente pela Ethernet do PS2

## Hospedagem sem pagar

O repositório público `maizon/MaizoOPLstore` hospeda **arquivos estáticos**.
O GitHub Actions baixa os ELFs dos projetos originais via HTTPS, verifica
tamanho e SHA-256 dos Releases, e gera `site/v1/catalog`,
`site/v1/apps/*.elf` e `site/credits.json`.
Sem Railway, VPS, PC ligado, celular, Raspberry Pi, domínio pago ou
servidor particular. Alternativa de CDN pública: jsDelivr.

Catálogo:
https://raw.githubusercontent.com/maizon/MaizoOPLstore/main/site/v1/catalog

Créditos, autores e licenças:
https://raw.githubusercontent.com/maizon/MaizoOPLstore/main/site/credits.json

Loja navegável no browser:
https://cdn.jsdelivr.net/gh/maizon/MaizoOPLstore@main/site/index.html

## Segurança da comunicação

Cliente PS2 desenvolvido com BearSSL TLS 1.2, SNI, validação de cadeia X.509,
DNSName do servidor e validade do certificado. As âncoras de confiança vêm de
um subconjunto **revisado** da lista Mozilla (certifi) em
`thirdparty/maizo_store_roots.pem`; script `gen_maizo_store_roots.py`
gera a estrutura BearSSL em C. Se o relógio do PS2 estiver incorreto, o cliente
tenta NTP usando Ethernet. Sem hora utilizável e/ou certificado válido,
o download é rejeitado. O app testa primeiro raw.githubusercontent.com:443,
depois cdn.jsdelivr.net:443.

**Defesa adicional:** a lista local `maizo_store_catalog.h` contém o
SHA-256 + tamanho **pré-aprovados no próprio ELF do OPL**. Um catálogo
interceptado, modificado ou uma atualização inesperada de upstream
não pode trocar silenciosamente o executável instalado. Exige uma nova
build para aprovar novos hashes. Os ELF são testados como MIPS/EE e
gravados em arquivo parcial, só substituído após a verificação.

**Limites:** PS2 sem fonte de aleatoriedade criptográfica forte. A mistura de
temporizadores para a entropia do TLS não equivale a /dev/urandom de sistemas
modernos; não usar a conexão para passwords/credenciais. O tempo vindo de NTP
pode ser adulterado na rede. O teste real em PS2 é indispensável.
Não alegar segurança ou compatibilidade absoluta.

## Interface

Na interface Supreme, Configurações → Rede → MAIZO Store. Atalho L3 na aba
Aplicativos. Use D-pad para selecionar, A para instalar, B para voltar;
quadrado atualiza a lista, triângulo edita host e porta.
O catálogo permite navegar por múltiplas páginas e mostra o responsável
por cada homebrew. Aplicativos instalados usam:
`mass0:/APPS/<id>/BOOT.ELF`, com `title.cfg`, reconhecidos pelo
scanner APPS do OPL. O arquivo original é protegido como `BOOT.BAK`
durante atualização.

## Créditos e redistribuição

Todo aplicativo aponta para repositório, tag e arquivo oficial no
`catalog_sources.json`, com créditos de autores e informações de licença.
Projetos sem autorização de redistribuição claramente confirmada ficam
**apenas como referência/links oficiais na loja web**, não como downloads
espelhados. Crédits e avisos estão em `site/credits.json` e na UI web.

**Cuidado:** algumas ports/engines exigem arquivos extras licenciados ou dados
originais de jogos (Quake, Doom, POPSLoader). Esses dados NÃO são fornecidos;
o ELF separado pode não funcionar sem eles.

## Limites técnicos

A compilação/testes automatizados não confirmam sucesso no hardware.
A implementação recebe HTTP/1.1 com Content-Length, não suporta chunked
nem HTTP redirects arbitrários. Se a hospedagem mudar o comportamento HTTP
ou cadeia de certificados, a loja mostrará erro em vez de instalar
executáveis não validados. Não há promessa de 99% sem validação física.
