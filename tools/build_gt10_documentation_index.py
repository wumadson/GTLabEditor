#!/usr/bin/env python3
"""Build deterministic, page-aware GT-10 documentation indexes offline.

The Owner's Manual and Patch List use their embedded text layers. The MIDI
Implementation has an invalid character map, so its text is supplied as
per-page OCR output produced locally from the canonical PDF. Runtime never
opens or parses a PDF; it reads only the generated JSON resources.
"""

import argparse
import hashlib
import json
import re
from pathlib import Path

import pdfplumber
from pypdf import PdfReader


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs" / "official" / "boss_gt10"
OUTPUT = ROOT / "ai" / "knowledge" / "data" / "boss_gt10"


def normalized(text):
    return re.sub(r"\s+", " ", text.replace("\u00ad", "")).strip()


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def page_text(reader, first, last):
    return normalized(" ".join(
        reader.pages[number - 1].extract_text() or ""
        for number in range(first, last + 1)
    ))


def chunk(document_id, item, body, source_kind="OfficialDocumentation"):
    return {
        "id": item["id"],
        "documentId": document_id,
        "title": item["title"],
        "section": item["section"],
        "pageStart": item["pages"][0],
        "pageEnd": item["pages"][1],
        "language": "en",
        "body": body,
        "answerEn": item.get("answerEn", ""),
        "answerPtBr": item.get("answerPtBr", ""),
        "aliases": item.get("aliases", []),
        "relatedTopics": item.get("topics", []),
        "relatedBlocks": item.get("blocks", []),
        "sourceKind": source_kind,
        "claims": item.get("claims", {}),
    }


MANUAL_SECTIONS = [
    dict(id="owners_output_select", title="Output Select",
         section="Making Settings for a Connected Device (Output Select)", pages=(25, 25),
         aliases=["output select", "saída", "saida", "amplifier output"],
         topics=["routing", "system"], blocks=["output"],
         answerEn="Output Select matches the GT-10 output processing to the connected device. It can be stored per patch or used as one system-wide setting.",
         answerPtBr="Output Select adapta o processamento de saída da GT-10 ao equipamento conectado. Ele pode usar uma configuração por patch ou uma configuração global do sistema."),
    dict(id="owners_tuner", title="Tuner",
         section="Tuning the Guitar (TUNER)", pages=(26, 28),
         aliases=["tuner", "tuning", "afinador", "afinar", "tuner out", "mute tuner"],
         topics=["system", "controllers"], blocks=["tuner"],
         answerEn="The tuner shows the nearest note and pitch difference. Its output can bypass processing or mute the signal, and tuner operation can be assigned to a CTL pedal.",
         answerPtBr="O afinador mostra a nota mais próxima e a diferença de afinação. A saída pode ficar em bypass ou mute, e a função pode ser atribuída a um pedal CTL."),
    dict(id="owners_patch", title="Patches",
         section="Selecting a Tone (Patch Change)", pages=(29, 31),
         aliases=["patch", "patches", "user patch", "preset patch", "banco", "bank", "category"],
         topics=["patch", "system"], blocks=[],
         answerEn="A patch is a set of effects and parameter settings. The GT-10 has 400 patches: writable User patches in U01–U50 and read-only Preset patches in P01–P50.",
         answerPtBr="Um patch reúne efeitos e parâmetros. A GT-10 possui 400 patches: User editáveis nos bancos U01–U50 e Preset somente leitura nos bancos P01–P50.",
         claims={"patch.count": 400}),
    dict(id="owners_effect_chain", title="Effect Chain",
         section="Changing the Connection Order of Effects (Effect Chain)", pages=(38, 38),
         aliases=["effect chain", "signal chain", "cadeia", "ordem dos efeitos", "split", "merge"],
         topics=["signal_chain", "routing"], blocks=["signal_chain"],
         answerEn="The FX CHAIN screen changes effect order and shows on/off and movable states. PREAMP channels A and B form two routes selected with Channel Select; the channels themselves cannot be moved.",
         answerPtBr="A tela FX CHAIN altera a ordem dos efeitos e mostra estados ligado/desligado e de movimentação. Os canais PREAMP A e B formam duas rotas escolhidas por Channel Select; os próprios canais não podem ser movidos."),
    dict(id="owners_patch_write", title="Patch Write",
         section="Saving a Patch (PATCH WRITE)", pages=(42, 43),
         aliases=["patch write", "write patch", "salvar patch", "gravar patch", "patch copy", "patch exchange", "patch initialize"],
         topics=["patch"], blocks=[],
         answerEn="PATCH WRITE saves edited settings to a User patch and replaces the previous contents at the destination. Preset patches must be saved to a User location.",
         answerPtBr="PATCH WRITE salva as alterações em um patch User e substitui o conteúdo anterior do destino. Um Preset precisa ser salvo em uma posição User."),
    dict(id="owners_quick_settings", title="User Quick Settings",
         section="Storing Settings by Effect (User Quick Settings)", pages=(44, 45),
         aliases=["quick setting", "quick settings", "configuração rápida", "configuracao rapida", "copy preamp a b"],
         topics=["patch", "routing"], blocks=["preamp", "delay", "reverb", "fx1", "fx2"],
         answerEn="User Quick Settings store settings for individual effects for reuse in other patches. The manual also documents copying or swapping PREAMP and speaker settings between channels A and B.",
         answerPtBr="User Quick Settings guardam configurações de efeitos individuais para reutilização em outros patches. O manual também descreve copiar ou trocar PREAMP e speaker entre os canais A e B."),
    dict(id="owners_controller_global", title="Global Pedal Functions",
         section="Using the CTL/EXP Pedal With the Same Functions Assigned at All Times", pages=(47, 47),
         aliases=["global pedal", "ctl global", "exp global", "pedal function", "função global", "funcao global"],
         topics=["controllers", "expression", "system"], blocks=["pedalboard"],
         answerEn="Controller Preference determines whether CTL, EXP and EXP SW use the same system-wide functions or the functions stored in each patch.",
         answerPtBr="Controller Preference define se CTL, EXP e EXP SW usam funções globais do sistema ou as funções armazenadas em cada patch."),
    dict(id="owners_pedal_fx", title="Patch Pedal Functions",
         section="Setting CTL/EXP Functions Individually in Each Patch (Pedal FX)", pages=(48, 49),
         aliases=["pedal fx", "ctl1", "ctl2", "exp sw", "expression switch", "pedal de expressão", "pedal de expressao"],
         topics=["controllers", "expression", "patch"], blocks=["pedalboard"],
         answerEn="With Controller Preference set to Patch, CTL1, CTL2, EXP PEDAL SW and EXP Pedal functions can be configured independently in each patch.",
         answerPtBr="Com Controller Preference em Patch, as funções de CTL1, CTL2, EXP PEDAL SW e EXP Pedal podem ser configuradas separadamente em cada patch."),
    dict(id="owners_assign", title="Assign 1–8",
         section="Setting Each Controller Functions to Individual Patches (Assign)", pages=(50, 54),
         aliases=["assign", "assigns", "atribuição", "atribuicao", "pra que serve assign", "como configurar um assign", "target", "target min", "target max", "source", "source mode", "toggle", "momentary", "moment", "controle"],
         topics=["assign", "controllers", "expression", "patch"], blocks=["assigns", "pedalboard"],
         answerEn="Each patch can store eight Assigns. Target chooses the controlled parameter; Min and Max define its range; Source chooses the controller. Moment applies the maximum only while a switch is held, while Toggle alternates between minimum and maximum on each press.",
         answerPtBr="Cada patch pode guardar oito Assigns. Target escolhe o parâmetro controlado; Min e Max definem a faixa; Source escolhe o controlador. Moment aplica o máximo apenas enquanto o switch está pressionado; Toggle alterna entre mínimo e máximo a cada acionamento.",
         claims={"assign.count": 8}),
    dict(id="owners_manual_mode", title="Manual Mode",
         section="Turning the Effects On and Off with the BANK/Number Pedals (Manual Mode)", pages=(55, 57),
         aliases=["manual mode", "foot 1", "foot 2", "foot 3", "foot 4", "bank up", "bank down", "pedais bank", "modo manual"],
         topics=["controllers", "system"], blocks=["pedalboard"],
         answerEn="Manual Mode uses the four number pedals and BANK pedals as assigned effect switches without changing the patch number. Their assignments are configured in MANUAL SETTING.",
         answerPtBr="Manual Mode usa os quatro pedais numéricos e os pedais BANK como switches de efeitos atribuídos, sem mudar o número do patch. As atribuições ficam em MANUAL SETTING."),
    dict(id="owners_phrase_loop", title="Phrase Loop",
         section="Phrase Loop Play", pages=(58, 60),
         aliases=["phrase loop", "loop", "rec play", "bank pedals loop"],
         topics=["controllers", "system"], blocks=["pedalboard"],
         answerEn="Phrase Loop records and repeatedly plays a phrase, with overdubbing available. It supports up to 38 seconds mono or 19 seconds stereo and can use BANK or assigned CTL pedals.",
         answerPtBr="Phrase Loop grava e repete uma frase, com overdub. Ele oferece até 38 segundos em mono ou 19 em estéreo e pode usar os pedais BANK ou CTL atribuídos."),
    dict(id="owners_preamp_system", title="System Preamp",
         section="Using the Identical Preamp Settings in All Patches (Preamp Mode)", pages=(69, 70),
         aliases=["system preamp", "preamp mode", "preamp global", "preamp a b"],
         topics=["routing", "system", "patch"], blocks=["preamp"],
         answerEn="Preamp Mode can use patch-specific preamps or one of three system preamps globally across patches.",
         answerPtBr="Preamp Mode pode usar os preamps específicos de cada patch ou um dos três System Preamp de forma global."),
    dict(id="owners_bank_change", title="Bank Change Mode",
         section="Setting the Timing Used for Switching Patches (Bank Change Mode)", pages=(72, 72),
         aliases=["bank change mode", "bank up", "bank down", "troca de banco"],
         topics=["controllers", "system", "patch"], blocks=["pedalboard"],
         answerEn="Bank Change Mode determines when a new patch is selected after using the BANK pedals.",
         answerPtBr="Bank Change Mode define em que momento um novo patch é selecionado após usar os pedais BANK."),
    dict(id="owners_exp_calibration", title="EXP Pedal Calibration",
         section="Adjusting the EXP Pedal", pages=(77, 77),
         aliases=["calibrate exp", "exp calibration", "calibrar exp", "calibrar pedal de expressão", "ajustar pedal de expressao"],
         topics=["expression", "controllers", "system"], blocks=["pedalboard"],
         answerEn="If the EXP Pedal no longer reaches its full range or EXP PEDAL SW does not switch correctly, open SYSTEM MENU > PDL CALIBRATION and follow the heel, toe and switch-sensitivity steps shown on screen.",
         answerPtBr="Se o EXP Pedal não atingir toda a faixa ou o EXP PEDAL SW não acionar corretamente, abra SYSTEM MENU > PDL CALIBRATION e siga na tela as etapas de calcanhar, ponta e sensibilidade do switch."),
    dict(id="owners_midi", title="Using External MIDI Devices",
         section="Using the GT-10 with External MIDI Devices Connected", pages=(79, 89),
         aliases=["midi", "program change", "bank select", "bulk dump", "bulk load", "device id", "midi channel"],
         topics=["midi", "system"], blocks=[],
         answerEn="The GT-10 supports MIDI channel settings, Program Change and Bank Select mapping, controller transmission, Bulk Dump and Bulk Load. Exclusive transfers use a matching Device ID.",
         answerPtBr="A GT-10 oferece canais MIDI, mapeamento de Program Change e Bank Select, transmissão de controladores, Bulk Dump e Bulk Load. Transferências Exclusive usam Device ID correspondente."),
    dict(id="owners_usb", title="Using the GT-10 via USB",
         section="Using the GT-10 Connected to a Computer Via USB", pages=(90, 92),
         aliases=["usb", "usb midi", "usb audio", "computer", "computador"],
         topics=["midi", "system"], blocks=[],
         answerEn="USB can carry bidirectional audio and, with the appropriate driver mode, bidirectional MIDI. The manual also documents USB input/output level settings.",
         answerPtBr="A conexão USB pode transportar áudio bidirecional e, no modo de driver adequado, MIDI bidirecional. O manual também documenta níveis de entrada e saída USB."),
    dict(id="owners_preamp_parameters", title="PREAMP Parameters",
         section="PREAMP", pages=(98, 98),
         aliases=["preamp", "dual l/r", "dual lr", "channel select", "channel delay", "dynamic sense", "dynamic sensitivity", "gain switch", "preamp a b"],
         topics=["routing", "signal_chain"], blocks=["preamp"],
         answerEn="PREAMP Channel Mode supports Single, Dual Mono, Dual L/R and Dynamic. Dual L/R sends channel A left and B right; Channel Delay delays B in dual modes; Dynamic Sensitivity sets when input level switches channels.",
         answerPtBr="O PREAMP oferece Single, Dual Mono, Dual L/R e Dynamic. Em Dual L/R, A sai à esquerda e B à direita; Channel Delay atrasa B nos modos duais; Dynamic Sensitivity define quando o nível de entrada troca os canais."),
    dict(id="owners_delay_parameters", title="DELAY Parameters",
         section="DELAY", pages=(119, 120),
         aliases=["delay", "atraso", "warp delay", "warp", "dual l/r delay", "dual series", "dual parallel"],
         topics=["delay"], blocks=["delay"],
         answerEn="Warp Delay controls feedback and delay volume together for an extreme delay effect. Rise Time sets how quickly it rises, Feedback Depth controls feedback, and Level Depth controls delay volume.",
         answerPtBr="Warp Delay controla em conjunto o feedback e o volume do delay para um efeito extremo. Rise Time define a velocidade da subida, Feedback Depth controla o feedback e Level Depth controla o volume."),
    dict(id="owners_reverb_parameters", title="REVERB Parameters",
         section="REVERB", pages=(121, 121),
         aliases=["reverb", "reverberação", "reverberacao", "reverb time", "density", "densidade", "spring sensitivity"],
         topics=["reverb"], blocks=["reverb"],
         answerEn="Reverb Time adjusts reverberation length, while Density adjusts the density of the reverb sound. The manual also defines Pre Delay, filters, effect/direct levels and Spring Sensitivity.",
         answerPtBr="Reverb Time ajusta a duração da reverberação, enquanto Density ajusta a densidade do som reverberado. O manual também define Pre Delay, filtros, níveis de efeito/direto e Spring Sensitivity."),
    dict(id="owners_assign_parameters", title="ASSIGN 1–8 Parameters",
         section="ASSIGN 1–8", pages=(126, 129),
         aliases=["assign parameters", "target parameter", "internal pedal", "wave pedal", "target min", "target max", "source mode"],
         topics=["assign", "controllers", "expression"], blocks=["assigns"],
         answerEn="ASSIGN 1–8 defines On/Off, Target, Min, Max, Source, Source Mode and active range. Additional parameters apply to Internal Pedal and Wave Pedal sources.",
         answerPtBr="ASSIGN 1–8 define On/Off, Target, Min, Max, Source, Source Mode e faixa ativa. Parâmetros adicionais se aplicam às fontes Internal Pedal e Wave Pedal."),
    dict(id="owners_system", title="System and Controller Parameters",
         section="System Parameters", pages=(134, 141),
         aliases=["system", "controller preference", "ctl3", "ctl4", "exp2", "foot controller", "global settings"],
         topics=["system", "controllers", "expression", "routing", "midi"], blocks=["pedalboard", "output"],
         answerEn="System parameters cover the operating environment, controller preferences, CTL1–4, EXP/EXP SW, MIDI controller output, Output Select and other global behavior.",
         answerPtBr="Os parâmetros System cobrem o ambiente de operação, preferências de controladores, CTL1–4, EXP/EXP SW, saída de controladores MIDI, Output Select e outros comportamentos globais."),
    dict(id="owners_troubleshooting", title="Troubleshooting",
         section="Error Messages and Troubleshooting", pages=(145, 146),
         aliases=["troubleshooting", "error", "midi offline", "usb offline", "solução de problemas", "problema"],
         topics=["system", "midi"], blocks=[],
         answerEn="The troubleshooting appendix explains display errors such as MIDI/USB buffer full or offline and provides checks for common sound and connection problems.",
         answerPtBr="O apêndice de solução de problemas explica erros como MIDI/USB buffer full ou offline e traz verificações para problemas comuns de som e conexão."),
]


def write_index(filename, document, chunks):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    payload = {"schemaVersion": 1, "document": document, "chunks": chunks}
    (OUTPUT / filename).write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def build_manual(path):
    reader = PdfReader(str(path))
    chunks = [chunk("owners_manual", item,
                    page_text(reader, item["pages"][0], item["pages"][1]))
              for item in MANUAL_SECTIONS]
    write_index("owners_manual_index.json", {
        "id": "owners_manual", "title": "GT-10 Owner's Manual",
        "language": "en", "sourceKind": "OfficialDocumentation",
        "pageCount": len(reader.pages), "sha256": sha256(path),
        "extraction": "embeddedText"
    }, chunks)


def build_patch_list(path):
    chunks = []
    with pdfplumber.open(path) as pdf:
        for page_number, page in enumerate(pdf.pages, 1):
            tables = page.extract_tables()
            if len(tables) != 1:
                raise RuntimeError(f"expected one patch table on page {page_number}")
            for row in tables[0][1:]:
                number, name, explanation, ctl1, ctl2, pickup = [normalized(v or "") for v in row]
                if not re.fullmatch(r"P\d{2}-[1-4]", number):
                    raise RuntimeError(f"invalid patch number on page {page_number}: {number}")
                body = f"{number} {name}. {explanation} CTL1: {ctl1}. CTL2: {ctl2}. Pickup: {pickup}."
                item = dict(
                    id=f"patch_{number.lower().replace('-', '_')}", title=f"{number} — {name}",
                    section="GT-10 Preset Patch List", pages=(page_number, page_number),
                    aliases=[number, name, f"patch {number}", f"preset {name}", "factory patch", "preset de fábrica"],
                    topics=["factory_patches", "patch"], blocks=[],
                    answerEn=f"The official Preset Patch List identifies {number} as {name}: {explanation}",
                    answerPtBr=f"A lista oficial de presets identifica {number} como {name}. Descrição oficial em inglês: {explanation}",
                )
                chunks.append(chunk("patch_list", item, body, "OfficialPatchReference"))
    if len(chunks) != 200:
        raise RuntimeError(f"expected 200 preset patches, extracted {len(chunks)}")
    write_index("patch_list_index.json", {
        "id": "patch_list", "title": "GT-10 Preset Patch List",
        "language": "en", "sourceKind": "OfficialPatchReference",
        "pageCount": 4, "sha256": sha256(path), "extraction": "embeddedTextTable"
    }, chunks)


def midi_section(page):
    if page == 1:
        return "Recognized Receive Data"
    if page == 2:
        return "RQ1/DT1 and Transmitted Data"
    if page == 3:
        return "Transmitted System Exclusive Messages"
    if page <= 44:
        return "Parameter Address Map"
    return "Supplementary Tables"


def build_midi(path, ocr_directory):
    reader = PdfReader(str(path))
    chunks = []
    for page in range(1, len(reader.pages) + 1):
        source = ocr_directory / f"page-{page:03d}.txt"
        if not source.is_file():
            raise RuntimeError(f"missing MIDI OCR page: {source}")
        body = normalized(source.read_text(encoding="utf-8"))
        if not body:
            raise RuntimeError(f"empty MIDI OCR page: {page}")
        aliases = ["midi implementation", "midi protocol"]
        answer_en = ""
        answer_pt = ""
        topics = ["midi"]
        if page == 1:
            aliases += ["recognized receive data", "control change", "bank select", "program change", "active sensing"]
            answer_en = "The MIDI Implementation documents received Bank Select, Control Change, Program Change, timing and Active Sensing messages, plus System Exclusive communication."
            answer_pt = "O MIDI Implementation documenta recepção de Bank Select, Control Change, Program Change, timing e Active Sensing, além da comunicação System Exclusive."
        elif page == 2:
            aliases += ["sysex", "system exclusive", "rq1", "request data", "dt1", "data set", "roland checksum"]
            answer_en = "The GT-10 uses Roland System Exclusive messages with model ID 00H 00H 2FH. RQ1 (11H) requests data and DT1 (12H) transfers data; messages include device ID, address, size or data, checksum and EOX."
            answer_pt = "A GT-10 usa mensagens Roland System Exclusive com model ID 00H 00H 2FH. RQ1 (11H) solicita dados e DT1 (12H) transfere dados; as mensagens incluem device ID, endereço, tamanho ou dados, checksum e EOX."
        elif page == 4:
            aliases += ["parameter address map", "address block map", "user patch address", "preset patch address", "quick setting address"]
            answer_en = "The Parameter Address Map uses 7-bit hexadecimal notation and defines blocks for System, MIDI, User and Preset patches, Quick Settings and the temporary area. The Editor backend remains authoritative for implemented addresses."
            answer_pt = "O Parameter Address Map usa notação hexadecimal de 7 bits e define blocos para System, MIDI, patches User/Preset, Quick Settings e área temporária. O backend do Editor continua sendo a autoridade para endereços implementados."
        item = dict(
            id=f"midi_page_{page:03d}", title=midi_section(page),
            section=midi_section(page), pages=(page, page), aliases=aliases,
            topics=topics, blocks=[], answerEn=answer_en, answerPtBr=answer_pt,
        )
        chunks.append(chunk("midi_implementation", item, body))
    write_index("midi_implementation_index.json", {
        "id": "midi_implementation", "title": "GT-10 MIDI Implementation",
        "language": "en", "sourceKind": "OfficialDocumentation",
        "pageCount": len(reader.pages), "sha256": sha256(path),
        "extraction": "localOCRBecauseEmbeddedCharacterMapIsInvalid",
        "ocrPages": [1, len(reader.pages)]
    }, chunks)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--midi-ocr-directory", type=Path, required=True,
                        help="Directory containing page-001.txt through page-048.txt")
    args = parser.parse_args()
    build_manual(DOCS / "gt10_owners_manual.pdf")
    build_patch_list(DOCS / "gt10_patch_list.pdf")
    build_midi(DOCS / "gt10_midi_implementation.pdf", args.midi_ocr_directory)


if __name__ == "__main__":
    main()
