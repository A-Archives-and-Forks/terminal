// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <dwrite_3.h>

#include <til/mutex.h>

struct NearbyFontCollection
{
    static wil::com_ptr<IDWriteFontCollection> GetFresh()
    {
        return _get(true);
    }

    wil::com_ptr<IDWriteFontCollection> GetCached()
    {
        const auto inner = collection.lock();
        if (!*inner)
        {
            *inner = _get(false);
        }
        return *inner;
    }

private:
    til::shared_mutex<wil::com_ptr<IDWriteFontCollection>> collection;
    
    static wil::com_ptr<IDWriteFontCollection> _get(bool forceUpdate)
    {
        // DWRITE_FACTORY_TYPE_SHARED _should_ return the same instance every time.
        wil::com_ptr<IDWriteFactory> factory;
        THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(factory), reinterpret_cast<::IUnknown**>(factory.addressof())));

        wil::com_ptr<IDWriteFontCollection> systemFontCollection;
        THROW_IF_FAILED(factory->GetSystemFontCollection(systemFontCollection.addressof(), forceUpdate));

        // IDWriteFactory5 is supported since Windows 10, build 15021.
        const auto factory5 = factory.try_query<IDWriteFactory5>();
        if (!factory5)
        {
            return systemFontCollection;
        }

        wil::com_ptr<IDWriteFontSet> systemFontSet;
        // IDWriteFontCollection1 is supported since Windows 7.
        THROW_IF_FAILED(systemFontCollection.query<IDWriteFontCollection1>()->GetFontSet(systemFontSet.addressof()));

        wil::com_ptr<IDWriteFontSetBuilder1> fontSetBuilder;
        THROW_IF_FAILED(factory5->CreateFontSetBuilder(fontSetBuilder.addressof()));
        THROW_IF_FAILED(fontSetBuilder->AddFontSet(systemFontSet.get()));

        {
            const std::filesystem::path module{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
            const auto folder{ module.parent_path() };

            for (const auto& p : std::filesystem::directory_iterator(folder))
            {
                if (til::ends_with(p.path().native(), L".ttf"))
                {
                    wil::com_ptr<IDWriteFontFile> fontFile;
                    if (SUCCEEDED_LOG(factory5->CreateFontFileReference(p.path().c_str(), nullptr, fontFile.addressof())))
                    {
                        LOG_IF_FAILED(fontSetBuilder->AddFontFile(fontFile.get()));
                    }
                }
            }
        }

        wil::com_ptr<IDWriteFontSet> fontSet;
        THROW_IF_FAILED(fontSetBuilder->CreateFontSet(fontSet.addressof()));

        wil::com_ptr<IDWriteFontCollection1> fontCollection;
        THROW_IF_FAILED(factory5->CreateFontCollectionFromFontSet(fontSet.get(), fontCollection.addressof()));

        return fontCollection;
    }
};
