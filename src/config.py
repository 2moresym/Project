from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = ROOT / "data"
CHATS_DIR = DATA_DIR / "chats"
HISTORY_FILE = DATA_DIR / "history.json"  # legacy compatibility
SETTINGS_FILE = DATA_DIR / "settings.json"
HF_ENDPOINT = "https://router.huggingface.co/v1/chat/completions"
DEFAULT_MODEL = "openai/gpt-oss-120b:groq"
DEFAULT_USER_AGENT = "Project-TinyAIPlayground/1.0 (Python urllib)"
DEFAULT_AI_NAME = "Vaxx"
MODELS = [
    "openai/gpt-oss-120b:groq",
    "openai/gpt-oss-120b:nscale",
    "openai/gpt-oss-120b:together",
    "openai/gpt-oss-20b:groq",
    "openai/gpt-oss-20b:nscale",
    "openai/gpt-oss-20b:together",
]
