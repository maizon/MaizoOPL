# MAIZO Store — hospedagem gratuita, sem servidor próprio

**Destino principal já publicado em GitHub como arquivos estáticos.**
O repositório público `maizon/MaizoOPLstore` é gratuito: GitHub Actions
baixa ELFs de Releases, confere SHA-256, publica em `site/v1/apps/*.elf`
e mantém `site/v1/catalog`. Não há Raspberry Pi, PC ligado, VPS, Railway,
credenciais, assinatura ou serviço próprio de backend.

URL pública do catálogo:
https://raw.githubusercontent.com/maizon/MaizoOPLstore/main/site/v1/catalog

Espelho jsDelivr:
https://cdn.jsdelivr.net/gh/maizon/MaizoOPLstore@main/site/v1/catalog

## PS2 com Ethernet e HTTPS

O MAIZO SUPREME agora tem código para cliente **HTTPS/TLS 1.2 via BearSSL**,
que é compilado para PS2 na build; a primeira abertura usa automaticamente
`raw.githubusercontent.com` na porta 443, sem configuração de host.
Caso necessário, TRIÂNGULO permite outro host/porta.
A rota do GitHub é construída automaticamente.

A cadeia de certificados do servidor NÃO é validada por este cliente,
pois não depende de relógio/CA do console. Isto significa que **a sessão
TLS isoladamente NÃO autentica o servidor**. Em compensação cada ELF só
é instalado quando seu comprimento exato e SHA-256 coincidem com valores
**fixados no código do aplicativo**. Não enviar credenciais/dados pessoais
pela conexão; catálogo e nomes são apenas informativos.

A biblioteca BearSSL 0.6 é baixada durante a compilação de
https://bearssl.org/bearssl-0.6.tar.gz e aceita somente o SHA-256
6705bba1714961b41a728dfc5debbe348d2966c117649392f8c8139efc83ff14.
Depende de curl e conexão HTTPS no ambiente de compilação.

## Uso

Abra Configurações → Rede → MAIZO Store ou aperte L3 na aba Aplicativos.
A loja exibe as aplicações gratuitas disponíveis; selecione uma e confirme.
O ELF verificado vai para `mass0:/APPS/<id>/BOOT.ELF`, com `title.cfg`,
reconhecido pela biblioteca APPS após atualização. Downloads parciais
`DOWNLOAD.PART` são apagados; atualizações salvam `BOOT.BAK` durante
a substituição. Há modo offline para visualizar itens conhecidos.

## Limitações

- A conexão HTTPS do PS2 requer teste em console físico; CI apenas verifica
  compilação e as rotinas SHA-256.
- O HTTP da rede não aceita transferências chunked nem redirecionamentos:
  apenas 200 + Content-Length com tamanho conhecido.
- Se o GitHub mudar seu endereço, protocolo TLS ou formas de resposta, uma
  nova build do cliente pode ser necessária.
- Sem download genérico de executáveis, ISOs ou ROMs. Somente os ELFs
  pré-aprovados e com SHA-256 compilado na aplicação.
- Os drivers de jogo/PadEmu não são alterados.
- GitHub Pages não é necessário, não há pagamentos nem conta Railway.
