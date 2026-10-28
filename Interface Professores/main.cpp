// Interface gráfica (Win32) do cadastro de alunos.
// A lógica e o banco de dados continuam no Python (Projeto_do_vitao.py):
// esta janela só chama o script e mostra os resultados.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <cmath>
#include <cwchar>
#include <string>
#include <tuple>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ---------------------------------------------------------------------------
// Configurações e layout
// ---------------------------------------------------------------------------
constexpr int LARGURA = 1180;
constexpr int ALTURA = 720;

constexpr RECT RC_LOUSA = { 20, 12, 1160, 170 };
constexpr RECT RC_FORM = { 20, 188, 400, 600 };
constexpr RECT RC_LISTA = { 420, 188, 1160, 600 };
constexpr int Y_MESA = 612;

constexpr int X_CAMPOS = 60;
constexpr int LARGURA_CAMPOS = 320;

enum : int {
    ID_NOME = 101, ID_RUA, ID_BAIRRO, ID_CEP,
    ID_NOTA1, ID_NOTA2, ID_NOTA3, ID_NOTA4,
    ID_LISTA, ID_ATUALIZAR,
};
// O botão "Cadastrar/Salvar" usa IDOK para funcionar com a tecla Enter,
// e o "Cancelar" usa IDCANCEL para funcionar com a tecla Esc.

constexpr UINT WM_CARREGAR_LISTA = WM_APP + 1;

// ---------------------------------------------------------------------------
// Estado global da janela
// ---------------------------------------------------------------------------
HINSTANCE g_instancia;
HWND g_campos[8];
HWND g_lista;
HWND g_botaoSalvar, g_botaoCancelar;
HFONT g_fonteUI, g_fonteRotulo, g_fonteTitulo, g_fontePequena, g_fonteGizGrande, g_fonteGiz;
std::wstring g_script;       // caminho completo do Projeto_do_vitao.py
int g_totalAlunos = -1;
std::wstring g_idEditando;   // vazio = cadastrando um aluno novo

// ---------------------------------------------------------------------------
// Utilidades de texto
// ---------------------------------------------------------------------------
std::wstring Utf8ParaWide(const std::string& texto)
{
    if (texto.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, texto.data(), (int)texto.size(), nullptr, 0);
    std::wstring r(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, texto.data(), (int)texto.size(), r.data(), n);
    return r;
}

std::string WideParaUtf8(const std::wstring& texto)
{
    if (texto.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, texto.data(), (int)texto.size(), nullptr, 0, nullptr, nullptr);
    std::string r(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, texto.data(), (int)texto.size(), r.data(), n, nullptr, nullptr);
    return r;
}

std::vector<std::wstring> Dividir(const std::wstring& texto, wchar_t separador)
{
    std::vector<std::wstring> partes;
    size_t inicio = 0;
    while (true) {
        size_t fim = texto.find(separador, inicio);
        std::wstring parte = texto.substr(inicio, fim == std::wstring::npos ? std::wstring::npos : fim - inicio);
        if (!parte.empty() && parte.back() == L'\r') parte.pop_back();
        partes.push_back(parte);
        if (fim == std::wstring::npos) break;
        inicio = fim + 1;
    }
    return partes;
}

std::wstring TextoDe(HWND controle)
{
    int n = GetWindowTextLengthW(controle);
    std::wstring t(n, L'\0');
    GetWindowTextW(controle, t.data(), n + 1);
    return t;
}

// "7.5" -> "7,5"
std::wstring FormatarNota(const std::wstring& texto)
{
    wchar_t buf[32];
    swprintf_s(buf, L"%.1f", wcstod(texto.c_str(), nullptr));
    std::wstring r = buf;
    for (auto& c : r) if (c == L'.') c = L',';
    return r;
}

// ---------------------------------------------------------------------------
// Ponte com o Python
// ---------------------------------------------------------------------------
bool ArquivoExiste(const std::wstring& caminho)
{
    DWORD a = GetFileAttributesW(caminho.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// Procura o script subindo a partir da pasta do .exe
// (funciona tanto rodando pelo Visual Studio quanto copiando o .exe para a pasta do script).
std::wstring LocalizarScript()
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring pasta = buf;
    pasta.resize(pasta.find_last_of(L"\\/"));

    for (int nivel = 0; nivel < 6; ++nivel) {
        for (const wchar_t* relativo : { L"\\Projeto_do_vitao.py", L"\\Projeto do vitao\\Projeto_do_vitao.py" }) {
            if (ArquivoExiste(pasta + relativo)) return pasta + relativo;
        }
        size_t pos = pasta.find_last_of(L"\\/");
        if (pos == std::wstring::npos) break;
        pasta.resize(pos);
    }
    return L"";
}

// Executa "python Projeto_do_vitao.py <comando>", envia `entrada` pelo stdin
// e devolve tudo o que o script imprimiu. Retorna false se o Python não pôde ser iniciado.
bool ExecutarPython(const wchar_t* comando, const std::string& entrada, std::string& saida, DWORD& codigoSaida)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE entradaLeitura, entradaEscrita, saidaLeitura, saidaEscrita;
    if (!CreatePipe(&saidaLeitura, &saidaEscrita, &sa, 0)) return false;
    if (!CreatePipe(&entradaLeitura, &entradaEscrita, &sa, 0)) {
        CloseHandle(saidaLeitura);
        CloseHandle(saidaEscrita);
        return false;
    }
    SetHandleInformation(saidaLeitura, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(entradaEscrita, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = entradaLeitura;
    si.hStdOutput = saidaEscrita;
    si.hStdError = saidaEscrita;

    std::wstring pastaScript = g_script.substr(0, g_script.find_last_of(L'\\'));
    PROCESS_INFORMATION pi = {};
    bool iniciou = false;
    for (const wchar_t* executavel : { L"python", L"py" }) {
        std::wstring linha = std::wstring(executavel) + L" \"" + g_script + L"\" " + comando;
        if (CreateProcessW(nullptr, linha.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                           nullptr, pastaScript.c_str(), &si, &pi)) {
            iniciou = true;
            break;
        }
    }

    CloseHandle(entradaLeitura);
    CloseHandle(saidaEscrita);
    if (!iniciou) {
        CloseHandle(entradaEscrita);
        CloseHandle(saidaLeitura);
        return false;
    }

    DWORD escritos;
    if (!entrada.empty()) WriteFile(entradaEscrita, entrada.data(), (DWORD)entrada.size(), &escritos, nullptr);
    CloseHandle(entradaEscrita);

    char buf[4096];
    DWORD lidos;
    while (ReadFile(saidaLeitura, buf, sizeof(buf), &lidos, nullptr) && lidos > 0) saida.append(buf, lidos);
    CloseHandle(saidaLeitura);

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &codigoSaida);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

void AvisarPythonIndisponivel(HWND janela)
{
    MessageBoxW(janela,
        g_script.empty()
            ? L"Não encontrei o arquivo Projeto_do_vitao.py.\n\nDeixe o .exe dentro da pasta do projeto ou na mesma pasta do script."
            : L"Não consegui executar o Python.\n\nVerifique se o Python está instalado e marcado no PATH.",
        L"Python não encontrado", MB_ICONERROR);
}

// ---------------------------------------------------------------------------
// Ações: listar, cadastrar e alterar
// ---------------------------------------------------------------------------
void AtualizarModoFormulario(HWND janela)
{
    bool editando = !g_idEditando.empty();
    SetWindowTextW(g_botaoSalvar, editando ? L"Salvar alterações" : L"Cadastrar aluno");
    SetWindowPos(g_botaoSalvar, nullptr, 0, 0, editando ? LARGURA_CAMPOS - 110 : LARGURA_CAMPOS, 38,
                 SWP_NOMOVE | SWP_NOZORDER);
    ShowWindow(g_botaoCancelar, editando ? SW_SHOW : SW_HIDE);

    RECT cabecalho = { RC_FORM.left, RC_FORM.top, RC_FORM.right, RC_FORM.top + 40 };
    InvalidateRect(janela, &cabecalho, FALSE);
}

void LimparFormulario(HWND janela)
{
    for (HWND campo : g_campos) SetWindowTextW(campo, L"");
    g_idEditando.clear();
    AtualizarModoFormulario(janela);
    SetFocus(g_campos[0]);
}

// Copia o aluno da linha clicada para o formulário.
void EditarAluno(HWND janela, int linha)
{
    wchar_t buf[256];
    ListView_GetItemText(g_lista, linha, 0, buf, 256);
    g_idEditando = buf;
    for (int i = 0; i < 8; ++i) {   // campo i corresponde à coluna i + 1 da lista
        ListView_GetItemText(g_lista, linha, i + 1, buf, 256);
        SetWindowTextW(g_campos[i], buf);
    }
    AtualizarModoFormulario(janela);
    SetFocus(g_campos[0]);
    SendMessageW(g_campos[0], EM_SETSEL, 0, -1);
}

void CarregarLista(HWND janela)
{
    if (g_script.empty()) { AvisarPythonIndisponivel(janela); return; }

    HCURSOR cursorAnterior = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    std::string saida;
    DWORD codigo = 0;
    bool ok = ExecutarPython(L"listar", "", saida, codigo);
    SetCursor(cursorAnterior);

    if (!ok) { AvisarPythonIndisponivel(janela); return; }
    if (codigo != 0) {
        MessageBoxW(janela, (L"O Python retornou um erro ao listar os alunos:\n\n" + Utf8ParaWide(saida)).c_str(),
                    L"Erro", MB_ICONERROR);
        return;
    }

    SendMessageW(g_lista, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_lista);
    int linha = 0;
    for (auto& texto : Dividir(Utf8ParaWide(saida), L'\n')) {
        auto campos = Dividir(texto, L'\t');
        if (campos.size() < 10) continue;

        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = linha;
        item.pszText = campos[0].data();
        ListView_InsertItem(g_lista, &item);

        for (int coluna = 1; coluna < 10; ++coluna) {
            std::wstring valor = coluna >= 5 ? FormatarNota(campos[coluna]) : campos[coluna];
            ListView_SetItemText(g_lista, linha, coluna, valor.data());
        }
        ++linha;
    }
    SendMessageW(g_lista, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_lista, nullptr, TRUE);

    g_totalAlunos = linha;
    RECT cabecalho = { RC_LISTA.left, RC_LISTA.top, RC_LISTA.right, RC_LISTA.top + 40 };
    InvalidateRect(janela, &cabecalho, FALSE);
}

void Salvar(HWND janela)
{
    bool editando = !g_idEditando.empty();
    std::wstring valores[8];
    for (int i = 0; i < 8; ++i) valores[i] = TextoDe(g_campos[i]);

    if (valores[0].find_first_not_of(L" \t") == std::wstring::npos) {
        MessageBoxW(janela, L"Digite o nome do aluno.", L"Campo obrigatório", MB_ICONWARNING);
        SetFocus(g_campos[0]);
        return;
    }
    if (g_script.empty()) { AvisarPythonIndisponivel(janela); return; }

    // No modo de edição, a primeira linha enviada ao Python é o ID do aluno.
    std::string entrada = editando ? WideParaUtf8(g_idEditando) + "\n" : "";
    for (auto& v : valores) entrada += WideParaUtf8(v) + "\n";

    HCURSOR cursorAnterior = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    std::string saida;
    DWORD codigo = 0;
    bool ok = ExecutarPython(editando ? L"alterar" : L"cadastrar", entrada, saida, codigo);
    SetCursor(cursorAnterior);
    if (!ok) { AvisarPythonIndisponivel(janela); return; }

    for (auto& linha : Dividir(Utf8ParaWide(saida), L'\n')) {
        if (linha.rfind(L"OK\t", 0) == 0) {
            std::wstring msg = L"Aluno \"" + valores[0] + (editando ? L"\" alterado com sucesso!" : L"\" cadastrado com sucesso!")
                             + L"\n\nMédia: " + FormatarNota(linha.substr(3));
            MessageBoxW(janela, msg.c_str(), editando ? L"Alteração salva" : L"Cadastro realizado", MB_ICONINFORMATION);
            LimparFormulario(janela);
            CarregarLista(janela);
            return;
        }
        if (linha.rfind(L"ERRO\t", 0) == 0) {
            MessageBoxW(janela, linha.substr(5).c_str(), editando ? L"Não foi possível alterar" : L"Não foi possível cadastrar",
                        MB_ICONWARNING);
            return;
        }
    }
    MessageBoxW(janela, (L"Resposta inesperada do Python:\n\n" + Utf8ParaWide(saida)).c_str(), L"Erro", MB_ICONERROR);
}

// ---------------------------------------------------------------------------
// Desenho do fundo (tema de professores)
// ---------------------------------------------------------------------------
void Retangulo(HDC dc, int l, int t, int r, int b, COLORREF cor)
{
    HBRUSH pincel = CreateSolidBrush(cor);
    RECT rc = { l, t, r, b };
    FillRect(dc, &rc, pincel);
    DeleteObject(pincel);
}

// Desenha uma forma com preenchimento e borda; `forma` recebe o DC já configurado.
template <typename F>
void ComPincel(HDC dc, COLORREF preenchimento, COLORREF borda, int espessura, F forma)
{
    HBRUSH pincel = CreateSolidBrush(preenchimento);
    HPEN caneta = CreatePen(PS_SOLID, espessura, borda);
    HGDIOBJ pincelAntigo = SelectObject(dc, pincel);
    HGDIOBJ canetaAntiga = SelectObject(dc, caneta);
    forma();
    SelectObject(dc, pincelAntigo);
    SelectObject(dc, canetaAntiga);
    DeleteObject(pincel);
    DeleteObject(caneta);
}

void Elipse(HDC dc, int l, int t, int r, int b, COLORREF preenchimento, COLORREF borda, int espessura = 1)
{
    ComPincel(dc, preenchimento, borda, espessura, [&] { Ellipse(dc, l, t, r, b); });
}

void Arredondado(HDC dc, int l, int t, int r, int b, int raio, COLORREF preenchimento, COLORREF borda, int espessura = 1)
{
    ComPincel(dc, preenchimento, borda, espessura, [&] { RoundRect(dc, l, t, r, b, raio, raio); });
}

void Poligono(HDC dc, std::initializer_list<POINT> pontos, COLORREF preenchimento, COLORREF borda)
{
    std::vector<POINT> p(pontos);
    ComPincel(dc, preenchimento, borda, 1, [&] { Polygon(dc, p.data(), (int)p.size()); });
}

void Linha(HDC dc, int x1, int y1, int x2, int y2, COLORREF cor, int espessura = 1)
{
    HPEN caneta = CreatePen(PS_SOLID, espessura, cor);
    HGDIOBJ antiga = SelectObject(dc, caneta);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, antiga);
    DeleteObject(caneta);
}

void Texto(HDC dc, const std::wstring& texto, RECT rc, HFONT fonte, COLORREF cor, UINT formato = DT_LEFT | DT_SINGLELINE | DT_VCENTER)
{
    HGDIOBJ antiga = SelectObject(dc, fonte);
    SetTextColor(dc, cor);
    DrawTextW(dc, texto.c_str(), -1, &rc, formato | DT_NOPREFIX);
    SelectObject(dc, antiga);
}

void DesenharParede(HDC dc)
{
    Retangulo(dc, 0, 0, LARGURA, Y_MESA, RGB(240, 228, 204));
    for (int x = 0; x < LARGURA; x += 40) Retangulo(dc, x, 0, x + 18, Y_MESA, RGB(235, 221, 194));
}

void DesenharEstrelaDeGiz(HDC dc, int cx, int cy, int raio, COLORREF cor)
{
    POINT p[11];
    for (int i = 0; i <= 10; ++i) {
        double ang = -3.14159265 / 2 + i * 3.14159265 / 5;
        int r = (i % 2 == 0) ? raio : raio / 2;
        p[i] = { cx + (int)(r * std::cos(ang)), cy + (int)(r * std::sin(ang)) };
    }
    HPEN caneta = CreatePen(PS_SOLID, 2, cor);
    HGDIOBJ antiga = SelectObject(dc, caneta);
    Polyline(dc, p, 11);
    SelectObject(dc, antiga);
    DeleteObject(caneta);
}

void DesenharLousa(HDC dc)
{
    const COLORREF giz = RGB(248, 248, 238);
    const COLORREF gizAmarelo = RGB(255, 236, 150);
    const COLORREF gizRosa = RGB(255, 190, 200);
    const RECT& r = RC_LOUSA;

    // Moldura de madeira e quadro verde
    Arredondado(dc, r.left, r.top, r.right, r.bottom, 12, RGB(133, 87, 46), RGB(92, 58, 28), 2);
    Retangulo(dc, r.left + 12, r.top + 12, r.right - 12, r.bottom - 12, RGB(38, 82, 58));

    // Marcas de giz apagado
    HPEN semBorda = (HPEN)GetStockObject(NULL_PEN);
    for (auto [x, y, w, h] : { std::tuple{ 120, 40, 160, 50 }, { 470, 110, 220, 35 }, { 820, 45, 140, 60 }, { 980, 105, 120, 30 } }) {
        HBRUSH mancha = CreateSolidBrush(RGB(46, 92, 67));
        HGDIOBJ p1 = SelectObject(dc, mancha), p2 = SelectObject(dc, semBorda);
        Ellipse(dc, x, y, x + w, y + h);
        SelectObject(dc, p1);
        SelectObject(dc, p2);
        DeleteObject(mancha);
    }

    SetBkMode(dc, TRANSPARENT);
    Texto(dc, L"Sala dos Professores", { r.left, r.top + 14, r.right, r.top + 78 }, g_fonteGizGrande, giz, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    Linha(dc, 440, r.top + 80, 740, r.top + 80, giz, 2);
    Texto(dc, L"Cadastro e lista de alunos", { r.left, r.top + 84, r.right, r.top + 120 }, g_fonteGiz, gizAmarelo, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Rabiscos de aula
    Texto(dc, L"A  B  C", { 60, 34, 300, 70 }, g_fonteGiz, giz);
    Texto(dc, L"2 + 2 = 4", { 60, 78, 300, 114 }, g_fonteGiz, gizRosa);
    Texto(dc, L"a² + b² = c²", { 880, 34, 1140, 70 }, g_fonteGiz, giz);
    Texto(dc, L"π ≈ 3,14", { 900, 78, 1140, 114 }, g_fonteGiz, gizAmarelo);
    DesenharEstrelaDeGiz(dc, 290, 62, 18, gizAmarelo);
    DesenharEstrelaDeGiz(dc, 860, 104, 13, giz);

    // Bandeja com giz e apagador
    Retangulo(dc, r.left + 40, r.bottom - 14, r.right - 40, r.bottom - 4, RGB(105, 66, 32));
    Arredondado(dc, 200, r.bottom - 22, 238, r.bottom - 14, 4, giz, RGB(200, 200, 190));
    Arredondado(dc, 250, r.bottom - 21, 278, r.bottom - 14, 4, gizAmarelo, RGB(210, 190, 120));
    Arredondado(dc, 960, r.bottom - 34, 1050, r.bottom - 14, 4, RGB(150, 105, 60), RGB(90, 60, 30));
    Retangulo(dc, 962, r.bottom - 22, 1048, r.bottom - 15, RGB(70, 70, 80));
}

void DesenharFolhaDeCaderno(HDC dc)
{
    const RECT& r = RC_FORM;
    Retangulo(dc, r.left + 5, r.top + 5, r.right + 5, r.bottom + 5, RGB(205, 188, 158));   // sombra
    Retangulo(dc, r.left, r.top, r.right, r.bottom, RGB(255, 253, 240));
    for (int y = r.top + 52; y < r.bottom - 6; y += 26) Linha(dc, r.left, y, r.right, y, RGB(205, 222, 240));
    Linha(dc, r.left + 30, r.top, r.left + 30, r.bottom, RGB(235, 150, 150), 2);
    for (int y : { r.top + 60, r.top + 205, r.top + 350 })
        Elipse(dc, r.left + 8, y, r.left + 20, y + 12, RGB(240, 228, 204), RGB(200, 185, 160));

    if (g_idEditando.empty())
        Texto(dc, L"Novo aluno", { X_CAMPOS, r.top + 4, r.right - 10, r.top + 36 }, g_fonteTitulo, RGB(40, 60, 110));
    else
        Texto(dc, L"Editando aluno nº " + g_idEditando, { X_CAMPOS, r.top + 4, r.right - 10, r.top + 36 },
              g_fonteTitulo, RGB(190, 90, 20));

    const COLORREF corRotulo = RGB(60, 60, 70);
    struct { const wchar_t* texto; int y; } rotulos[] = {
        { L"Nome do aluno *", 230 }, { L"Rua", 282 }, { L"Bairro", 334 },
        { L"CEP (somente números)", 386 }, { L"Notas", 438 },
    };
    for (auto& rot : rotulos)
        Texto(dc, rot.texto, { X_CAMPOS, rot.y, X_CAMPOS + LARGURA_CAMPOS, rot.y + 20 }, g_fonteRotulo, corRotulo);

    Texto(dc, L"Nota em branco conta como 0. Aceita 7,5 ou 7.5.",
          { X_CAMPOS, 552, r.right - 10, 590 }, g_fontePequena, RGB(120, 120, 130), DT_LEFT | DT_WORDBREAK);
}

void DesenharPainelLista(HDC dc)
{
    const RECT& r = RC_LISTA;
    Retangulo(dc, r.left + 5, r.top + 5, r.right + 5, r.bottom + 5, RGB(205, 188, 158));
    Retangulo(dc, r.left, r.top, r.right, r.bottom, RGB(255, 255, 255));
    Retangulo(dc, r.left, r.top, r.right, r.top + 6, RGB(52, 90, 160));

    Texto(dc, L"Alunos cadastrados", { r.left + 16, r.top + 10, r.left + 260, r.top + 42 }, g_fonteTitulo, RGB(40, 60, 110));
    if (g_totalAlunos >= 0) {
        std::wstring total = std::to_wstring(g_totalAlunos) + (g_totalAlunos == 1 ? L" aluno" : L" alunos");
        Texto(dc, total, { r.left + 250, r.top + 12, r.left + 400, r.top + 42 }, g_fonteRotulo, RGB(120, 120, 130));
    }
    Texto(dc, L"Clique duas vezes para alterar", { r.left + 360, r.top + 12, r.right - 140, r.top + 42 },
          g_fontePequena, RGB(120, 120, 130));
}

void DesenharMesa(HDC dc)
{
    // Tampo da mesa visto de cima
    Retangulo(dc, 0, Y_MESA, LARGURA, ALTURA, RGB(176, 118, 64));
    Retangulo(dc, 0, Y_MESA, LARGURA, Y_MESA + 4, RGB(130, 84, 42));
    Retangulo(dc, 0, ALTURA - 8, LARGURA, ALTURA, RGB(130, 84, 42));
    for (auto [x1, x2, y] : { std::tuple{ 0, 380, 630 }, { 520, 1180, 640 }, { 200, 900, 662 }, { 0, 600, 700 }, { 760, 1180, 690 } })
        Linha(dc, x1, y, x2, y, RGB(160, 104, 54));

    // Maçã para o professor
    Elipse(dc, 50, 640, 110, 698, RGB(205, 32, 42), RGB(140, 20, 25), 2);
    Elipse(dc, 62, 650, 76, 666, RGB(238, 110, 110), RGB(238, 110, 110));
    Linha(dc, 80, 642, 84, 628, RGB(90, 55, 25), 4);
    Poligono(dc, { { 85, 634 }, { 94, 622 }, { 110, 622 }, { 99, 634 } }, RGB(70, 160, 70), RGB(40, 110, 40));

    // Pilha de livros
    auto livro = [&](int l, int t, int r, int b, COLORREF capa) {
        Retangulo(dc, l, t, r, b, capa);
        Retangulo(dc, r - 10, t + 3, r - 2, b - 3, RGB(250, 246, 230));
        Retangulo(dc, l + 14, t + 6, l + 50, b - 6, RGB(220, 185, 90));
    };
    livro(140, 688, 300, 708, RGB(52, 90, 160));
    livro(152, 668, 290, 688, RGB(200, 90, 40));
    livro(162, 648, 284, 668, RGB(60, 130, 80));

    // Lápis
    Retangulo(dc, 330, 680, 348, 694, RGB(240, 140, 160));
    Retangulo(dc, 348, 679, 360, 695, RGB(190, 190, 200));
    Retangulo(dc, 360, 680, 480, 694, RGB(250, 200, 40));
    Linha(dc, 360, 687, 480, 687, RGB(225, 175, 30));
    Poligono(dc, { { 480, 680 }, { 504, 687 }, { 480, 694 } }, RGB(235, 205, 160), RGB(200, 170, 120));
    Poligono(dc, { { 497, 685 }, { 506, 687 }, { 497, 689 } }, RGB(60, 60, 60), RGB(60, 60, 60));

    // Plaquinha de mesa
    Arredondado(dc, 520, 632, 860, 704, 10, RGB(95, 60, 30), RGB(70, 44, 20));
    Arredondado(dc, 532, 642, 848, 694, 6, RGB(218, 180, 84), RGB(160, 120, 40), 2);
    Texto(dc, L"♥  Obrigado, professores!  ♥", { 532, 642, 848, 694 }, g_fonteTitulo, RGB(80, 55, 20), DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Caneca "A+"
    ComPincel(dc, RGB(255, 255, 255), RGB(150, 150, 160), 5, [&] {
        HGDIOBJ antigo = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, 948, 652, 980, 688);
        SelectObject(dc, antigo);
    });
    Arredondado(dc, 900, 640, 960, 706, 10, RGB(255, 255, 255), RGB(150, 150, 160), 2);
    Elipse(dc, 904, 640, 956, 652, RGB(110, 65, 30), RGB(150, 150, 160));
    Texto(dc, L"A+", { 900, 656, 960, 700 }, g_fonteTitulo, RGB(200, 30, 40), DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Globo
    Elipse(dc, 1050, 696, 1110, 712, RGB(80, 55, 30), RGB(60, 40, 20));
    Linha(dc, 1080, 684, 1080, 700, RGB(80, 55, 30), 5);
    Elipse(dc, 1048, 622, 1112, 686, RGB(60, 130, 200), RGB(40, 90, 150), 2);
    Elipse(dc, 1060, 634, 1084, 656, RGB(90, 170, 90), RGB(90, 170, 90));
    Elipse(dc, 1080, 650, 1104, 676, RGB(90, 170, 90), RGB(90, 170, 90));
    Elipse(dc, 1062, 664, 1074, 676, RGB(90, 170, 90), RGB(90, 170, 90));
    HGDIOBJ pincel = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HPEN aro = CreatePen(PS_SOLID, 3, RGB(170, 140, 60));
    HGDIOBJ caneta = SelectObject(dc, aro);
    Arc(dc, 1042, 616, 1118, 692, 1080, 692, 1080, 616);
    SelectObject(dc, caneta);
    SelectObject(dc, pincel);
    DeleteObject(aro);
}

void DesenharFundo(HDC dc)
{
    SetBkMode(dc, TRANSPARENT);
    DesenharParede(dc);
    DesenharLousa(dc);
    DesenharFolhaDeCaderno(dc);
    DesenharPainelLista(dc);
    DesenharMesa(dc);
}

// ---------------------------------------------------------------------------
// Criação dos controles
// ---------------------------------------------------------------------------
HFONT CriarFonte(int altura, int peso, const wchar_t* nome)
{
    return CreateFontW(-altura, 0, 0, 0, peso, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, nome);
}

HWND CriarCampo(HWND janela, int id, int x, int y, int largura, int limite, const wchar_t* dica, DWORD estiloExtra = 0)
{
    HWND campo = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | estiloExtra,
                                 x, y, largura, 26, janela, (HMENU)(INT_PTR)id, g_instancia, nullptr);
    SendMessageW(campo, WM_SETFONT, (WPARAM)g_fonteUI, TRUE);
    SendMessageW(campo, EM_LIMITTEXT, limite, 0);
    SendMessageW(campo, EM_SETCUEBANNER, FALSE, (LPARAM)dica);
    return campo;
}

void CriarControles(HWND janela)
{
    g_fonteUI = CriarFonte(16, FW_NORMAL, L"Segoe UI");
    g_fonteRotulo = CriarFonte(15, FW_SEMIBOLD, L"Segoe UI");
    g_fonteTitulo = CriarFonte(22, FW_BOLD, L"Segoe UI");
    g_fontePequena = CriarFonte(13, FW_NORMAL, L"Segoe UI");
    g_fonteGizGrande = CriarFonte(40, FW_BOLD, L"Segoe Print");
    g_fonteGiz = CriarFonte(22, FW_NORMAL, L"Segoe Print");

    g_campos[0] = CriarCampo(janela, ID_NOME, X_CAMPOS, 250, LARGURA_CAMPOS, 80, L"Ex.: Maria da Silva");
    g_campos[1] = CriarCampo(janela, ID_RUA, X_CAMPOS, 302, LARGURA_CAMPOS, 100, L"Ex.: Rua das Flores, 123");
    g_campos[2] = CriarCampo(janela, ID_BAIRRO, X_CAMPOS, 354, LARGURA_CAMPOS, 60, L"Ex.: Centro");
    g_campos[3] = CriarCampo(janela, ID_CEP, X_CAMPOS, 406, LARGURA_CAMPOS, 8, L"Ex.: 01001000", ES_NUMBER);

    const wchar_t* dicasNotas[] = { L"Nota 1", L"Nota 2", L"Nota 3", L"Nota 4" };
    for (int i = 0; i < 4; ++i)
        g_campos[4 + i] = CriarCampo(janela, ID_NOTA1 + i, X_CAMPOS + i * 84, 458, 68, 5, dicasNotas[i], ES_CENTER);

    g_botaoSalvar = CreateWindowExW(0, L"BUTTON", L"Cadastrar aluno",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                    X_CAMPOS, 505, LARGURA_CAMPOS, 38, janela, (HMENU)IDOK, g_instancia, nullptr);
    SendMessageW(g_botaoSalvar, WM_SETFONT, (WPARAM)g_fonteRotulo, TRUE);

    g_botaoCancelar = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                                      X_CAMPOS + LARGURA_CAMPOS - 100, 505, 100, 38,
                                      janela, (HMENU)IDCANCEL, g_instancia, nullptr);
    SendMessageW(g_botaoCancelar, WM_SETFONT, (WPARAM)g_fonteUI, TRUE);

    HWND botaoAtualizar = CreateWindowExW(0, L"BUTTON", L"Atualizar lista",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                          RC_LISTA.right - 130, RC_LISTA.top + 13, 115, 28,
                                          janela, (HMENU)ID_ATUALIZAR, g_instancia, nullptr);
    SendMessageW(botaoAtualizar, WM_SETFONT, (WPARAM)g_fonteUI, TRUE);

    g_lista = CreateWindowExW(0, WC_LISTVIEWW, L"",
                              WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                              RC_LISTA.left + 16, RC_LISTA.top + 50, (RC_LISTA.right - RC_LISTA.left) - 32, 346,
                              janela, (HMENU)ID_LISTA, g_instancia, nullptr);
    SendMessageW(g_lista, WM_SETFONT, (WPARAM)g_fonteUI, TRUE);
    ListView_SetExtendedListViewStyle(g_lista, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    struct { const wchar_t* titulo; int largura; int alinhamento; } colunas[] = {
        { L"ID", 34, LVCFMT_LEFT }, { L"Nome", 130, LVCFMT_LEFT }, { L"Rua", 95, LVCFMT_LEFT },
        { L"Bairro", 85, LVCFMT_LEFT }, { L"CEP", 90, LVCFMT_LEFT },
        { L"N1", 46, LVCFMT_CENTER }, { L"N2", 46, LVCFMT_CENTER },
        { L"N3", 46, LVCFMT_CENTER }, { L"N4", 46, LVCFMT_CENTER },
        { L"Média", 58, LVCFMT_CENTER },
    };
    for (int i = 0; i < (int)std::size(colunas); ++i) {
        LVCOLUMNW c = {};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        c.pszText = (LPWSTR)colunas[i].titulo;
        c.cx = colunas[i].largura;
        c.fmt = colunas[i].alinhamento;
        ListView_InsertColumn(g_lista, i, &c);
    }
}

// ---------------------------------------------------------------------------
// Janela principal
// ---------------------------------------------------------------------------
LRESULT CALLBACK ProcedimentoJanela(HWND janela, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        CriarControles(janela);
        PostMessageW(janela, WM_CARREGAR_LISTA, 0, 0);
        return 0;

    case WM_CARREGAR_LISTA:
        CarregarLista(janela);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) Salvar(janela);
        else if (LOWORD(wParam) == IDCANCEL && !g_idEditando.empty()) LimparFormulario(janela);
        else if (LOWORD(wParam) == ID_ATUALIZAR) CarregarLista(janela);
        return 0;

    case WM_NOTIFY: {
        auto* aviso = (NMITEMACTIVATE*)lParam;
        if (aviso->hdr.hwndFrom == g_lista && aviso->hdr.code == NM_DBLCLK && aviso->iItem >= 0)
            EditarAluno(janela, aviso->iItem);
        break;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(janela, &ps);
        HDC memoria = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, LARGURA, ALTURA);
        HGDIOBJ antigo = SelectObject(memoria, bitmap);
        DesenharFundo(memoria);
        BitBlt(dc, 0, 0, LARGURA, ALTURA, memoria, 0, 0, SRCCOPY);
        SelectObject(memoria, antigo);
        DeleteObject(bitmap);
        DeleteDC(memoria);
        EndPaint(janela, &ps);
        return 0;
    }

    case WM_DESTROY:
        for (HFONT f : { g_fonteUI, g_fonteRotulo, g_fonteTitulo, g_fontePequena, g_fonteGizGrande, g_fonteGiz })
            DeleteObject(f);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(janela, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instancia, HINSTANCE, PWSTR, int mostrar)
{
    g_instancia = instancia;
    g_script = LocalizarScript();

    // O Python herda estas variáveis e passa a falar UTF-8 com a interface.
    SetEnvironmentVariableW(L"PYTHONUTF8", L"1");
    SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW classe = { sizeof(classe) };
    classe.lpfnWndProc = ProcedimentoJanela;
    classe.hInstance = instancia;
    classe.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    classe.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    classe.lpszClassName = L"InterfaceProfessores";
    RegisterClassExW(&classe);

    const DWORD estilo = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT tamanho = { 0, 0, LARGURA, ALTURA };
    AdjustWindowRect(&tamanho, estilo, FALSE);
    int w = tamanho.right - tamanho.left, h = tamanho.bottom - tamanho.top;

    HWND janela = CreateWindowExW(0, classe.lpszClassName, L"Sala dos Professores — Cadastro de Alunos", estilo,
                                  (GetSystemMetrics(SM_CXSCREEN) - w) / 2, (GetSystemMetrics(SM_CYSCREEN) - h) / 2,
                                  w, h, nullptr, nullptr, instancia, nullptr);
    ShowWindow(janela, mostrar);
    UpdateWindow(janela);

    MSG mensagem;
    while (GetMessageW(&mensagem, nullptr, 0, 0)) {
        if (!IsDialogMessageW(janela, &mensagem)) {   // Tab entre campos e Enter para cadastrar
            TranslateMessage(&mensagem);
            DispatchMessageW(&mensagem);
        }
    }
    return (int)mensagem.wParam;
}
