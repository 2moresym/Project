import json
import os
from dataclasses import dataclass
from typing import Protocol
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
from .config import DEFAULT_MODEL, DEFAULT_USER_AGENT, HF_ENDPOINT

class AIProvider(Protocol):
    def reply(self, messages: list[dict[str, str]]) -> str: ...

@dataclass
class DemoProvider:
    def reply(self, messages: list[dict[str, str]]) -> str:
        user = messages[-1].get("content", "").strip() if messages else ""
        if user.lower() in {"hello", "hi", "hey"}:
            return "Hey! I'm the tiny AI playground."
        if "who are you" in user.lower():
            return "I'm Project, a small Python AI playground running in your terminal."
        return f"Demo backend received: {user}\n\nSet HF_TOKEN to use a real model."

@dataclass
class HuggingFaceProvider:
    token: str
    model: str = DEFAULT_MODEL
    user_agent: str = DEFAULT_USER_AGENT

    def reply(self, messages: list[dict[str, str]]) -> str:
        payload = json.dumps({"model": self.model, "messages": messages, "temperature": 0.7, "max_tokens": 512, "stream": False}).encode()
        request = Request(HF_ENDPOINT, data=payload, headers={"Authorization": f"Bearer {self.token}", "Content-Type": "application/json", "Accept": "application/json", "User-Agent": self.user_agent}, method="POST")
        try:
            with urlopen(request, timeout=120) as response:
                data = json.loads(response.read().decode())
        except HTTPError as exc:
            raise RuntimeError(f"Hugging Face HTTP {exc.code}: {exc.read().decode(errors='replace')}") from exc
        except URLError as exc:
            raise RuntimeError(f"Network error: {exc.reason}") from exc
        except TimeoutError as exc:
            raise RuntimeError("Hugging Face request timed out.") from exc
        try:
            content = data["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as exc:
            raise RuntimeError(f"Unexpected Hugging Face response: {data}") from exc
        if not isinstance(content, str):
            raise RuntimeError(f"Unexpected Hugging Face response: {data}")
        return content.strip()

def make_provider(model: str | None = None) -> AIProvider:
    token = os.environ.get("HF_TOKEN", "").strip()
    selected = (model or os.environ.get("HF_MODEL", DEFAULT_MODEL)).strip() or DEFAULT_MODEL
    ua = os.environ.get("HF_USER_AGENT", DEFAULT_USER_AGENT).strip() or DEFAULT_USER_AGENT
    return HuggingFaceProvider(token, selected, ua) if token else DemoProvider()
