import unittest
from src.chat import Chat
from src.providers import DemoProvider
from src.sessions import safe_name

class CoreTests(unittest.TestCase):
    def test_memory_and_search(self):
        chat = Chat(DemoProvider())
        self.assertTrue(chat.remember("My name is Vexx"))
        chat.messages = [
            {"role": "user", "content": "hello minecraft"},
            {"role": "assistant", "content": "hello"},
        ]
        self.assertEqual(len(chat.search("minecraft")), 1)

    def test_auto_memory(self):
        chat = Chat(DemoProvider())
        self.assertEqual(chat.auto_remember("I like Minecraft."), ["I like Minecraft"])
        self.assertFalse(chat.auto_remember("hello there"))

    def test_safe_chat_name(self):
        self.assertEqual(safe_name("My Cool Chat!"), "My_Cool_Chat_")
        self.assertEqual(safe_name("   "), "chat")

if __name__ == "__main__":
    unittest.main()
