import os
import sqlite3
import sys

# O banco fica sempre ao lado deste arquivo, não importa de onde o script for chamado
# (terminal ou interface em C++).
CAMINHO_BANCO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "alunos.db")

conexao = sqlite3.connect(CAMINHO_BANCO)
cursor = conexao.cursor()


cursor.execute("""
CREATE TABLE IF NOT EXISTS alunos (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    nome TEXT NOT NULL,
    rua TEXT,
    bairro TEXT,
    cep TEXT,
    nota1 REAL,
    nota2 REAL,
    nota3 REAL,
    nota4 REAL,
    media REAL
)
""")


def converter_nota(texto):
    # Aceita "7,5" ou "7.5"; vazio vira 0
    texto = texto.strip().replace(",", ".")
    nota = float(texto or 0)
    if nota < 0:
        raise ValueError("As notas não podem ser negativas.")
    return nota


def inserir_aluno(cursor, conexao, nome, rua, bairro, cep, notas):
    if not nome.strip():
        raise ValueError("O nome do aluno é obrigatório.")

    Media = sum(notas) / 4

    cursor.execute("""
    INSERT INTO ALUNOS (
    nome, rua , bairro , cep,
    nota1,nota2,nota3,nota4, media
    )
    VALUES(?,?,?,?,?,?,?,?,?)
    """, (nome.strip(), rua.strip(), bairro.strip(), cep.strip(), *notas, Media))
    conexao.commit()
    return Media


def atualizar_aluno(cursor, conexao, id_aluno, nome, rua, bairro, cep, notas):
    if not nome.strip():
        raise ValueError("O nome do aluno é obrigatório.")

    Media = sum(notas) / 4

    cursor.execute("""
    UPDATE alunos SET
    nome = ?, rua = ?, bairro = ?, cep = ?,
    nota1 = ?, nota2 = ?, nota3 = ?, nota4 = ?, media = ?
    WHERE id = ?
    """, (nome.strip(), rua.strip(), bairro.strip(), cep.strip(), *notas, Media, id_aluno))
    if cursor.rowcount == 0:
        raise ValueError("Aluno não encontrado.")
    conexao.commit()
    return Media


def buscar_alunos(cursor):
    cursor.execute("SELECT id, nome, rua, bairro, cep, nota1, nota2, nota3, nota4, media FROM alunos")
    return cursor.fetchall()


def Cadastrar_alunos(cursor, conexao):
    nome = input("Digite o nome do aluno: ")
    rua = input("Digite a rua onde o aluno mora: ")
    bairro = input("Digite o bairro do aluno: ")
    cep = input("Digite o CEP do aluno sem traços: ")
    try:
        notas = [
            converter_nota(input("Digite a primeira nota do aluno: ")),
            converter_nota(input("Digite a segunda nota do aluno: ")),
            converter_nota(input("Digite a terceira nota do aluno: ")),
            converter_nota(input("Digite a quarta nota do aluno: ")),
        ]
        Media = inserir_aluno(cursor, conexao, nome, rua, bairro, cep, notas)
    except ValueError as erro:
        print(f"\nErro: {erro}\n")
        return

    print("\nAluno cadastrado com sucesso!")
    print(f"Média do aluno: {Media:.2f}\n")


def listar_alunos(cursor):
    alunos = buscar_alunos(cursor)
    for aluno in alunos:
        print(f"ID {aluno[0]} - {aluno[1]} | Média: {aluno[9]:.1f}")


def Alterar_alunos(cursor, conexao):
    listar_alunos(cursor)
    try:
        id_aluno = int(input("\nDigite o ID do aluno que deseja alterar: "))
    except ValueError:
        print("\nErro: digite um número de ID válido.\n")
        return

    cursor.execute("SELECT nome, rua, bairro, cep, nota1, nota2, nota3, nota4 FROM alunos WHERE id = ?", (id_aluno,))
    atual = cursor.fetchone()
    if atual is None:
        print("\nErro: aluno não encontrado.\n")
        return

    print("\nPressione Enter para manter o valor atual.")
    nome = input(f"Nome [{atual[0]}]: ") or atual[0]
    rua = input(f"Rua [{atual[1] or ''}]: ") or (atual[1] or "")
    bairro = input(f"Bairro [{atual[2] or ''}]: ") or (atual[2] or "")
    cep = input(f"CEP [{atual[3] or ''}]: ") or (atual[3] or "")
    try:
        notas = []
        for i in range(4):
            texto = input(f"Nota {i + 1} [{atual[4 + i]}]: ")
            notas.append(converter_nota(texto) if texto.strip() else (atual[4 + i] or 0))
        Media = atualizar_aluno(cursor, conexao, id_aluno, nome, rua, bairro, cep, notas)
    except ValueError as erro:
        print(f"\nErro: {erro}\n")
        return

    print("\nAluno alterado com sucesso!")
    print(f"Nova média do aluno: {Media:.2f}\n")


def menu_terminal():
    print("------------- Cadastro de alunos -------------")
    print("Aviso: Caso o aluno não possua nota, digite 0 ou pressione Enter.\n")

    while True:
        print("Escolha a opção que deseja: ")
        print("1 - Cadastrar alunos.")
        print("2 - Listar alunos.")
        print("3 - Alterar dados dos alunos:")
        print("4 - sair")

        opcao = input("O que pretende fazer? ")

        if opcao == "1":
            Cadastrar_alunos(cursor, conexao)

        elif opcao == "2":
            listar_alunos(cursor)

        elif opcao == "3":
            Alterar_alunos(cursor, conexao)

        elif opcao == "4":
            print("Saindo...")
            break

        else:
            print("Favor digitar somente os numeros destinados: 1 a 4")

    conexao.close()
    print("Programa encerrado!")


# ---------------------------------------------------------------------------
# Modo backend: usado pela interface em C++.
#   python Projeto_do_vitao.py listar
#       -> uma linha por aluno, campos separados por TAB
#   python Projeto_do_vitao.py cadastrar
#       -> lê 8 linhas da entrada (nome, rua, bairro, cep, nota1..nota4)
#       -> responde "OK<TAB>media" ou "ERRO<TAB>mensagem"
#   python Projeto_do_vitao.py alterar
#       -> igual ao cadastrar, mas a primeira linha é o ID do aluno
# Toda a comunicação é em UTF-8 para os acentos funcionarem.
# ---------------------------------------------------------------------------
def modo_backend(comando):
    sys.stdin.reconfigure(encoding="utf-8")
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")

    def limpar(texto):
        return str(texto).replace("\t", " ").replace("\r", " ").replace("\n", " ")

    if comando == "listar":
        for aluno in buscar_alunos(cursor):
            print("\t".join(limpar("" if campo is None else campo) for campo in aluno))

    elif comando in ("cadastrar", "alterar"):
        campos = sys.stdin.read().replace("﻿", "").split("\n")  # remove BOM, se houver
        id_texto = campos.pop(0) if comando == "alterar" else ""
        campos += [""] * (8 - len(campos))
        try:
            notas = [converter_nota(n) for n in campos[4:8]]
            if comando == "cadastrar":
                Media = inserir_aluno(cursor, conexao, *campos[0:4], notas)
            else:
                Media = atualizar_aluno(cursor, conexao, int(id_texto), *campos[0:4], notas)
            print(f"OK\t{Media:.2f}")
        except ValueError as erro:
            mensagem = str(erro)
            if "could not convert" in mensagem:
                mensagem = "As notas devem ser números (ex.: 7 ou 7,5)."
            elif "invalid literal for int" in mensagem:
                mensagem = "ID do aluno inválido."
            print(f"ERRO\t{limpar(mensagem)}")

    else:
        print(f"ERRO\tComando desconhecido: {limpar(comando)}")

    conexao.close()


if __name__ == "__main__":
    if len(sys.argv) > 1:
        modo_backend(sys.argv[1])
    else:
        menu_terminal()
