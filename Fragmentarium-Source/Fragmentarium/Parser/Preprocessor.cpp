#include "Preprocessor.h"

#include <QStringList>
#include <QRegExp>
#include <QMap>
#include <QFileInfo>
#include <QDir>


#include "../../SyntopiaCore/Exceptions/Exception.h"
#include "../../SyntopiaCore/Logging/Logging.h"
#include "../../SyntopiaCore/Math/Random.h"

using namespace SyntopiaCore::Exceptions;
using namespace SyntopiaCore::Logging;
using namespace SyntopiaCore::Math;


namespace Fragmentarium {
	namespace Parser {	

		namespace {
			float parseFloat(QString s) {
				bool succes = false;
				double to = s.toDouble(&succes);
				if (!succes	) {
					WARNING("Could not parse float: " + s);
					return 0;
				}
				return to;
			};

			Vector3f parseVector3f(QString s1, QString s2, QString s3) {
				return Vector3f(parseFloat(s1), parseFloat(s2), parseFloat(s3));
			};


			void ParseSource(FragmentSource* fs,QString input, QFile* file) {
				fs->sourceFiles.append(file);
				int sf = fs->sourceFiles.count()-1;

				QStringList in = input.split(QRegExp("\r\n|\r|\n"));
				QList<int> lines;
				for (int i = 0; i < in.count(); i++) lines.append(i);

				QList<int> source;
				for (int i = 0; i < in.count(); i++) source.append(sf);

				QRegExp includeCommand("^#include\\s\"([^\\s]+)\"\\s*$"); // Look for #include "test.frag"

				for (int i = 0; i < in.count(); i++) {
					if (includeCommand.indexIn(in[i]) != -1) {	
						QString fileName =  includeCommand.cap(1);
						QFile* f = new QFile(fileName);
						if (!QFileInfo(fileName).isAbsolute() && file) {
							QDir d = QFileInfo(*file).absolutePath();
							delete(f);
							f = new QFile(d.absoluteFilePath(fileName));
						} 
						if (!f->open(QIODevice::ReadOnly | QIODevice::Text))
							throw Exception("Unable to open: " +  QFileInfo(*f).absoluteFilePath());

						INFO("Including file: " + QFileInfo(*f).absolutePath());
						QString a = f->readAll();
						ParseSource(fs, a, f);
					} else {
						fs->lines.append(lines[i]);
						fs->sourceFile.append(source[i]);
						fs->source.append(in[i]);
					}
				}
			}
		}

		FragmentSource::FragmentSource() : hasPixelSizeUniform(false) {};

		// We leak here, but fs's are copied!
		FragmentSource::~FragmentSource() { /*foreach (QFile* f, sourceFiles) delete(f);*/ }

		FragmentSource Preprocessor::Parse(QString input, QFile* file, bool moveMain) {

			FragmentSource fs;

			// Step one: resolve includes:
			ParseSource(&fs, input, file);

			// Step two: resolve magic uniforms:
			// (pixelsize, 
			QRegExp includeCommand("^\\s*uniform\\s+vec2\\s+pixelSize.*$"); // Look for 'uniform vec2 pixelSize'
			if (fs.source.indexOf(includeCommand)!=-1) {
				fs.hasPixelSizeUniform = true;
			} 

			// Look for 'uniform float varName; slider[0.1;1;2.0]'
			QRegExp float3Slider("^\\s*uniform\\s+vec3\\s+(\\S+)\\s*;\\s*slider\\[\\((\\S+),(\\S+),(\\S+)\\),\\((\\S+),(\\S+),(\\S+)\\),\\((\\S+),(\\S+),(\\S+)\\)\\].*$"); 
			QRegExp colorChooser("^\\s*uniform\\s+vec3\\s+(\\S+)\\s*;\\s*color\\[(\\S+),(\\S+),(\\S+)\\].*$"); 
			QRegExp floatSlider("^\\s*uniform\\s+float\\s+(\\S+)\\s*;\\s*slider\\[(\\S+),(\\S+),(\\S+)\\].*$"); 
			QRegExp intSlider("^\\s*uniform\\s+int\\s+(\\S+)\\s*;\\s*slider\\[(\\S+),(\\S+),(\\S+)\\].*$"); 
			QRegExp main("^\\s*void\\s+main\\s*\\(.*$"); 

			QString lastComment;
			QString currentGroup;
			for (int i = 0; i < fs.source.count(); i++) {
				QString s = fs.source[i];
				if (s.trimmed().startsWith("#camera")) {
					fs.source[i] = "// " + s;
					QString c = s.remove("#camera");
					fs.camera = c.trimmed();
				} else if (s.trimmed().startsWith("#group")) {
					fs.source[i] = "// " + s;
					QString c = s.remove("#group");
					currentGroup = c.trimmed();
				} else if (s.trimmed().startsWith("#info")) {
					fs.source[i] = "// " + s;
					QString c = s.remove("#info");
					INFO(c);
				} else if (moveMain && main.indexIn(s) != -1) {
					INFO("Found main: " + s );
					fs.source[i] = s.replace(" main", " fragmentariumMain");
				} else if (floatSlider.indexIn(s) != -1) {
					QString name = floatSlider.cap(1);
					fs.source[i] = "uniform float " + name + ";";
					QString fromS = floatSlider.cap(2);
					QString defS = floatSlider.cap(3);
					QString toS = floatSlider.cap(4);

					bool succes = false;
					double from = fromS.toDouble(&succes);
					bool succes2 = false;
					double def = defS.toDouble(&succes2);
					bool succes3 = false;
					double to = toS.toDouble(&succes3);
					if (!succes || !succes2 || !succes3) {
						WARNING("Could not parse interval for uniform: " + name);
						continue;
					}

					FloatParameter* fp= new FloatParameter(currentGroup, name, lastComment, from, to, def);
					fs.params.append(fp);
				} else if (float3Slider.indexIn(s) != -1) {

					QString name = float3Slider.cap(1);
					fs.source[i] = "uniform vec3 " + name + ";";
					Vector3f from = parseVector3f(float3Slider.cap(2), float3Slider.cap(3), float3Slider.cap(4));
					Vector3f defaults = parseVector3f(float3Slider.cap(5), float3Slider.cap(6), float3Slider.cap(7));
					Vector3f to = parseVector3f(float3Slider.cap(8), float3Slider.cap(9), float3Slider.cap(10));

					Float3Parameter* fp= new Float3Parameter(currentGroup, name, lastComment, from, to, defaults);
					fs.params.append(fp);
				} else if (colorChooser.indexIn(s) != -1) {

					QString name = colorChooser.cap(1);
					fs.source[i] = "uniform vec3 " + name + ";";
					Vector3f defaults = parseVector3f(colorChooser.cap(2), colorChooser.cap(3), colorChooser.cap(4));
					
					ColorParameter* cp= new ColorParameter(currentGroup, name, lastComment, defaults);
					fs.params.append(cp);
				} else if (intSlider.indexIn(s) != -1) {

					QString name = intSlider.cap(1);
					fs.source[i] = "uniform int " + name + ";";
					QString fromS = intSlider.cap(2);
					QString defS = intSlider.cap(3);
					QString toS = intSlider.cap(4);

					bool succes = false;
					int from = fromS.toInt(&succes);
					bool succes2 = false;
					int def = defS.toInt(&succes2);
					bool succes3 = false;
					int to = toS.toInt(&succes3);
					if (!succes || !succes2 || !succes3) {
						WARNING("Could not parse interval for uniform: " + name);
						continue;
					}

					IntParameter* ip= new IntParameter(currentGroup, name, lastComment, from, to, def);
					fs.params.append(ip);
				}

				if (s.trimmed().startsWith("//")) {
					QString c = s.remove("//");
					lastComment = c.trimmed();
				} else {
					lastComment = "";
				}
			}

			// To ensure main is called as the last command.
			if (moveMain) {
				fs.source.append("");
				fs.source.append("void main() { fragmentariumMain(); }");
				fs.source.append("");
			}

			return fs;
		}
	}
}

