#pragma once

#if defined background_library_exporting
#include <QtCore/QtGlobal>
#define background_library Q_DECL_EXPORT
#elif defined background_library_importing
#include <QtCore/QtGlobal>
#define background_library Q_DECL_IMPORT
#else
#define background_library
#endif
