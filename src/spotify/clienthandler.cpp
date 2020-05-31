#include "clienthandler.hpp"

using namespace spt;

ClientHandler::ClientHandler(const Settings &settings, QWidget *parent) : settings(settings), parent(parent)
{
	path = settings.spotify.path;
	process = new QProcess();
}

ClientHandler::~ClientHandler()
{
	if (process == nullptr)
		return;
	process->close();
	delete process;
	process = nullptr;
}

QString ClientHandler::start()
{
	// Don't start if already running
	if (isRunning())
		return QString();
	// Check if empty
	if (path.isEmpty())
		return "path is empty";
	// Check if path exists
	QFileInfo info(path);
	if (!info.exists())
		return "file in path does not exist";
	// If using global config, just start
	if (settings.spotify.globalConfig)
	{
		process->start(path);
		return QString();
	}
	// Check if username exists
	auto username = settings.spotify.username;
	if (username.isEmpty())
		return "no username provided";
	// Ask for password
	auto password = QInputDialog::getText(parent,
		"Enter password",
		QString("Enter password for Spotify user \"%1\":").arg(username),
		QLineEdit::Password);
	if (password.isEmpty())
		return "no password provided";
	// Attempt to start spotifyd
	QStringList arguments({
		"--bitrate",		QString::number(settings.spotify.bitrate),
		"--device-name",	"spotify-qt",
		"--username",		username,
		"--password",		password
	});
	if (supportsPulse())
		arguments.append({
			"--backend", "pulseaudio"
		});
	else
		qDebug() << "warning: spotifyd was compiled without pulseaudio support";
	process->start(path, arguments);
	return QString();
}

QString ClientHandler::clientExec(const QString &path, const QStringList &arguments)
{
	// Check if it exists
	QFileInfo file(path);
	if (!file.exists())
		return QString();
	// Check if either client
	if (file.baseName() != "spotifyd")
		return QString();
	// Prepare process
	QProcess process;
	// Get version info
	process.start(file.absoluteFilePath(), arguments);
	process.waitForFinished();
	// Entire stdout is version
	return process.readAllStandardOutput().trimmed();
}

bool ClientHandler::supportsPulse()
{
	return clientExec(path, {
		"--help"
	}).contains("pulseaudio");
}

QString ClientHandler::version(const QString &path)
{
	return clientExec(path, {
		"--version"
	});
}

bool ClientHandler::isRunning()
{
	if (path.isEmpty() || !QFile("/usr/bin/ps").exists())
		return false;
	QProcess ps;
	ps.start("/usr/bin/ps", {"aux"});
	ps.waitForFinished();
	auto out = ps.readAllStandardOutput();
	return QString(out).contains(path);
}

float ClientHandler::getVolume()
{
	return 0;
}

void ClientHandler::setVolume(float value)
{
	QProcess process;
	// Find what sink to use
	process.start("/usr/bin/pactl", {
		"list", "sink-inputs"
	});
	process.waitForFinished();
	auto sinks = QString(process.readAllStandardOutput()).split("Sink Input #");
	QString sink;
	for (auto &s : sinks)
		if (s.contains("Spotify"))
			sink = s;
	if (sink.isEmpty())
		return;
	// Sink was found, get id
	auto left = sink.left(sink.indexOf('\n'));
	auto sinkId = left.right(left.length() - left.lastIndexOf('#') - 1);
	process.start("/usr/bin/pactl", {
		"set-sink-input-volume", sinkId, QString::number(value, 'f', 2)
	});
	process.waitForFinished();
}