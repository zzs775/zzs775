#pragma once

#include <vsg/all.h>
#include <vsg/io/read.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/state/material.h>
#include <vsg/text/Text.h>
#include <vsg/text/Font.h>
#include <vsg/text/StandardLayout.h>
#include <vsg/utils/Builder.h>
#include <vsgXchange/all.h>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <map>
#include <string>

class FixMaterialVisitor : public vsg::Visitor {
public:
    void apply(vsg::Object& object) override;
    void apply(vsg::StateGroup& sg) override;
};

struct ForceBrightMaterialVisitor : public vsg::Visitor
{
    void apply(vsg::Object& object) override
    {
        object.traverse(*this);
    }

    void apply(vsg::Data& data) override
    {
        if (auto* phongValue = data.cast<vsg::PhongMaterialValue>())
        {
            vsg::PhongMaterial material = phongValue->value();
            material.diffuse   = vsg::vec4(1.0f, 0.95f, 0.8f, 1.0f);
            material.emissive  = vsg::vec4(0.15f, 0.15f, 0.15f, 1.0f);
            material.specular  = vsg::vec4(0.8f, 0.8f, 0.8f, 1.0f);
            material.shininess = 32.0f;
            phongValue->set(material);
        }
        else if (auto* pbrValue = data.cast<vsg::PbrMaterialValue>())
        {
            vsg::PbrMaterial material = pbrValue->value();
            material.baseColorFactor = vsg::vec4(1.0f, 0.95f, 0.8f, 1.0f);
            material.emissiveFactor  = vsg::vec4(0.15f, 0.15f, 0.15f, 1.0f);
            material.metallicFactor  = 0.1f;
            material.roughnessFactor = 0.5f;
            pbrValue->set(material);
        }
        data.traverse(*this);
    }
};

inline void forceBrightMaterial(vsg::Node* node)
{
    ForceBrightMaterialVisitor visitor;
    node->accept(visitor);
}

class ModelFactory {
public:
    static ModelFactory* instance();
    
    void init(vsg::ref_ptr<vsg::Options> options);
    
    vsg::ref_ptr<vsg::Node> getRawModel(const std::string& filename);
    
    vsg::ref_ptr<vsg::Node> createLabel(const QString& text, double height);
    
    vsg::ref_ptr<vsg::Node> createVisualEntity(
        const std::string& filename, 
        const QString& label, 
        double targetSize = 100.0
    );
    
    bool isInitialized() const { return _initialized; }
    bool hasFont() const { return _font.valid(); }
    
private:
    ModelFactory() = default;
    ~ModelFactory() = default;
    ModelFactory(const ModelFactory&) = delete;
    ModelFactory& operator=(const ModelFactory&) = delete;
    
    vsg::ref_ptr<vsg::Font> _font;
    vsg::ref_ptr<vsg::Options> _options;
    std::map<std::string, vsg::ref_ptr<vsg::Node>> _modelCache;
    bool _initialized = false;
    bool _vsgXchangeLoaded = false;
    
    vsg::dbox computeBoundingBox(vsg::ref_ptr<vsg::Node> node);
    double computeScaleFactor(const vsg::dbox& box, double targetSize);
};
