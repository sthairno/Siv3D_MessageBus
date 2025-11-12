#include <Siv3D.hpp>
#include <MessageBus/MessageBus.hpp>

void Main()
{
	MessageBus::MessageBus bus(U"localhost", 6379);

	bus.subscribe(U"test1");
	bus.subscribe(U"test2");

	Font font{ 20 };

	while (System::Update())
	{
		bus.tick();

		if (bus.isConnected())
		{
			font(U"接続済み").draw(20, 20, Palette::Green);
		}
		else if (bus.error())
		{
			font(bus.error()).draw(20, 20, Palette::Red);
		}
		else
		{
			font(U"未接続").draw(20, 20, Palette::White);
		}

		for (const auto& event : bus.events())
		{
			Print << U"イベントを受信: " << event.channel << U" value=" << event.value;
		}

		if (SimpleGUI::Button(U"Emit: test1", Vec2{ 20, 50 }))
		{
			bus.emit(U"test1", U"Hello from test1");
		}

		if (SimpleGUI::Button(U"Emit: test2", Vec2{ 20, 90 }))
		{
			bus.emit(U"test2", U"Hello from test2");
		}
	}
}
