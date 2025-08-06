export module Log;

export namespace Log
{
	void Init();
	void Shutdown();

	void Write(const char* fmt, ...);
	void Warning(const char* format, ...);
	void Error(const char* format, ...);
	void Success(const char* format, ...);
}