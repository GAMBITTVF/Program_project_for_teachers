# Sala dos Professores — Cadastro de Alunos

Sistema simples para professores cadastrarem alunos, registrarem quatro notas e acompanharem a média de cada um.

O projeto tem duas partes que trabalham juntas:

| Parte | Linguagem | O que faz |
|---|---|---|
| `Projeto do vitao/Projeto_do_vitao.py` | Python | Toda a lógica e o banco de dados (SQLite). Pode ser usado sozinho pelo terminal. |
| `Interface Professores/main.cpp` | C++ (Win32) | Interface gráfica para Windows com tema de sala de aula. Não guarda nada: só chama o script Python e mostra o resultado. |

## Funcionalidades

- **Cadastrar aluno** com nome, rua, bairro, CEP e quatro notas
- **Listar alunos** com todas as notas e a média
- **Alterar dados** de um aluno já cadastrado
- **Cálculo automático da média**: soma das 4 notas ÷ 4
- Notas aceitam vírgula ou ponto (`7,5` ou `7.5`). Nota em branco conta como `0`.
- Validações: o nome é obrigatório, as notas não podem ser negativas e têm que ser números.

## Requisitos

- **Python 3.7 ou mais novo** instalado e no `PATH`. No instalador do Windows, marque a opção *"Add Python to PATH"*. Não precisa instalar nenhuma biblioteca extra, porque o `sqlite3` já vem com o Python.
- **Para a interface gráfica:** Windows e o Visual Studio com a carga de trabalho *"Desenvolvimento para desktop com C++"* instalada (o projeto usa C++20).

## Como usar

### Opção 1: pela interface gráfica (Windows)

1. Abra o arquivo `Projeto do vitao.slnx` no Visual Studio.
2. Defina **Interface Professores** como projeto de inicialização: clique com o botão direito no projeto e escolha *"Definir como Projeto de Inicialização"*.
3. Pressione **F5** para compilar e executar.

Na janela:

- Preencha o formulário à esquerda e clique em **Cadastrar aluno**, ou aperte **Enter**.
- A lista à direita mostra todos os alunos. Clique em **Atualizar lista** para recarregar.
- **Clique duas vezes num aluno** para editar os dados dele. Depois clique em **Salvar alterações**, ou em **Cancelar** (atalho **Esc**) para desistir.
- Use **Tab** para passar de um campo para o outro.

> O `.exe` procura o `Projeto_do_vitao.py` na própria pasta e nas pastas acima dela. Por isso funciona tanto rodando pelo Visual Studio quanto copiando o `.exe` para a pasta do script.

### Opção 2: pelo terminal

```bash
cd "Projeto do vitao"
python Projeto_do_vitao.py
```

Vai aparecer um menu:

```
1 - Cadastrar alunos.
2 - Listar alunos.
3 - Alterar dados dos alunos:
4 - sair
```

## Como funciona por dentro

```
┌──────────────────────────┐   python Projeto_do_vitao.py <comando>   ┌───────────────────────┐        ┌───────────┐
│ Interface Professores    │ ───────────────────────────────────────▶ │ Projeto_do_vitao.py   │ ─────▶ │ alunos.db │
│ (C++ / Win32)            │ ◀─────────────────────────────────────── │ (lógica + validações) │        │ (SQLite)  │
└──────────────────────────┘          resposta pelo stdout            └───────────────────────┘        └───────────┘
```

A interface em C++ não acessa o banco diretamente. A cada ação, ela executa o script Python em segundo plano, sem abrir janela de console. Os dados vão pela entrada padrão (stdin) e a resposta volta pela saída padrão (stdout). Toda a comunicação é em UTF-8, para que os acentos funcionem.

### Comandos do modo "backend"

Quando o script recebe um argumento, ele não mostra o menu e responde como um serviço:

| Comando | Entrada (stdin) | Saída (stdout) |
|---|---|---|
| `listar` | nenhuma | Uma linha por aluno, com os campos separados por TAB: `id, nome, rua, bairro, cep, nota1, nota2, nota3, nota4, media` |
| `cadastrar` | 8 linhas: nome, rua, bairro, cep, nota1, nota2, nota3, nota4 | `OK<TAB>media` ou `ERRO<TAB>mensagem` |
| `alterar` | 9 linhas: **id** + as mesmas 8 do `cadastrar` | `OK<TAB>media` ou `ERRO<TAB>mensagem` |

Exemplo:

```bash
printf "Maria da Silva\nRua das Flores, 123\nCentro\n01001000\n8\n7,5\n9\n10\n" | python Projeto_do_vitao.py cadastrar
# OK	8.62
```

### Banco de dados

O arquivo `alunos.db` é criado automaticamente na mesma pasta do script na primeira execução. Ele tem uma única tabela:

```sql
CREATE TABLE alunos (
    id     INTEGER PRIMARY KEY AUTOINCREMENT,
    nome   TEXT NOT NULL,
    rua    TEXT,
    bairro TEXT,
    cep    TEXT,
    nota1  REAL,
    nota2  REAL,
    nota3  REAL,
    nota4  REAL,
    media  REAL
);
```

> O `alunos.db` está no `.gitignore` para que dados de alunos não sejam enviados ao repositório.

## Estrutura do projeto

```
.
├── Projeto do vitao.slnx              # Solução do Visual Studio
├── Interface Professores/
│   ├── main.cpp                       # Interface gráfica (Win32)
│   └── Interface Professores.vcxproj
└── Projeto do vitao/
    ├── Projeto_do_vitao.py            # Lógica, menu do terminal e modo backend
    ├── Projeto do vitao.pyproj
    └── alunos.db                      # (gerado automaticamente, não versionado)
```

## Problemas comuns

| Mensagem | Solução |
|---|---|
| *"Não consegui executar o Python"* | Instale o Python e marque *"Add Python to PATH"*. Depois reinicie o Visual Studio. |
| *"Não encontrei o arquivo Projeto_do_vitao.py"* | Deixe o `.exe` dentro da pasta do projeto ou na mesma pasta do script. |
| *"As notas devem ser números"* | Use apenas números nas notas, como `7` ou `7,5`. |
